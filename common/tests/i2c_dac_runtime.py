#!/usr/bin/env python3
"""Run shared production I2C polling with DAC-only and mixed device configurations."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from i2c_fake_sdk import make_fake_sdk

root=Path(__file__).resolve().parents[2]
source=r'''
#include <assert.h>
#include <string.h>
#include "i2c_io.c"
i2c_inst_t fake_ports[2]={{0},{1}};
static uint64_t now;
static unsigned init_count, event_count, fail_address=255, short_address=255;
static struct { uint8_t address, bytes[3]; unsigned len; } events[512];
uint64_t time_us_64(void) {return now;}
void gpio_set_function(unsigned pin,unsigned mode) {assert(pin<30 && mode==GPIO_FUNC_I2C);}
void gpio_pull_up(unsigned pin) {assert(pin<30);}
unsigned i2c_init(i2c_inst_t *p,unsigned hz) {assert(p->id==0 && hz==400000); ++init_count; return hz;}
int i2c_write_timeout_us(i2c_inst_t *p,uint8_t address,const uint8_t *bytes,
                        size_t len,bool nostop,unsigned timeout) {
    assert(p->id==0 && timeout==1000 && len<=3);
    if(address==fail_address) return -1;
    if(address==short_address) return (int)len-1;
    if(!nostop) {
        assert(event_count<512);
        events[event_count].address=address;
        events[event_count].len=len;
        memcpy(events[event_count++].bytes,bytes,len);
    }
    return len;
}
int i2c_read_timeout_us(i2c_inst_t *p,uint8_t address,uint8_t *bytes,
                       size_t len,bool nostop,unsigned timeout) {
    assert(p->id==0 && (address==0x20 || address==0x21) && len==1 && !nostop && timeout==1000);
    if(address==fail_address) return -1;
    *bytes=0xa5; return 1;
}
static void expect_last(unsigned n,uint32_t code) {
    uint8_t expected[3];
    unsigned len=sn_i2c_dac_encode(dac_channels[n].type,dac_channels[n].channel,code,expected);
    for(unsigned j=event_count;j>0;--j) {
        unsigned i=j-1;
        if(events[i].address!=dac_channels[n].address) continue;
        if(dac_channels[n].type>=SN_DAC_AD5337 && events[i].bytes[0]!=(1u<<dac_channels[n].channel)) continue;
        assert(events[i].len==len && !memcmp(expected,events[i].bytes,len)); return;
    }
    assert(!"DAC write missing");
}
int main(void) {
    uint32_t inputs,status;
    sn_i2c_io_setup();
    assert(enabled && init_count==1); // shared bus initialized once
    sn_i2c_io_poll(false);
    for(unsigned n=0;n<I2C_DAC_CHANNEL_COUNT;++n) expect_last(n,dac_channels[n].safe_code);
    const uint32_t codes[]={0xabc,0x123,0x456,0x789};
    unsigned before=event_count;
    sn_i2c_dac_commands(codes);
    sn_i2c_io_packet(0,&inputs,&status);
    assert(event_count==before); // no hardware I/O in packet path
    unsigned all=((1u<<I2C_DAC_CHANNEL_COUNT)-1);
    assert((status&0x0f0f0000)==((all<<16)|(all<<24)));
#if I2C_IO_ENABLE
    assert((status&0x303)==0x303);
#endif
    sn_i2c_io_poll(true); assert(event_count==before);
    now=10000; sn_i2c_io_poll(true);
    for(unsigned n=0;n<I2C_DAC_CHANNEL_COUNT;++n) expect_last(n,codes[n]);
    fail_address=dac_channels[0].address;
    now=20000; sn_i2c_io_poll(true);
    sn_i2c_io_packet(0,&inputs,&status);
    assert(!(status&0x01010000));
#if I2C_IO_ENABLE
    assert((status&0x303)==0x303); // faulted DAC cannot corrupt digital status
#endif
    fail_address=255; short_address=dac_channels[0].address;
    now=30000; sn_i2c_io_poll(true);
    sn_i2c_io_packet(0,&inputs,&status); assert(!(status&0x01010000));
    short_address=255; before=event_count;
    now=40000; sn_i2c_io_poll(true);
    sn_i2c_io_packet(0,&inputs,&status);
    assert((status&0x0f0f0000)==((all<<16)|(all<<24)));
    // First retry write restores safe state before the command.
    uint8_t safe[3]; unsigned len=sn_i2c_dac_encode(dac_channels[0].type,0,dac_channels[0].safe_code,safe);
    unsigned found=0;
    for(unsigned i=before;i<event_count;++i) if(events[i].address==dac_channels[0].address) {
        assert(events[i].len==len && !memcmp(events[i].bytes,safe,len));found=1;break;
    }
    assert(found);
#if I2C_IO_ENABLE
    fail_address=0x20; now=50000; sn_i2c_io_poll(true);
    sn_i2c_io_packet(0,&inputs,&status);
    assert((status&0x303)==0x202 && (status&0x0f0f0000)==((all<<16)|(all<<24)));
    fail_address=255;
#endif
    sn_i2c_io_poll(false); // must override period on disconnect
    for(unsigned n=0;n<I2C_DAC_CHANNEL_COUNT;++n) expect_last(n,dac_channels[n].safe_code);
    puts("PASS: shared bus ownership, DAC-only/mixed polling, snapshot isolation, NAK/short-write, recovery and disconnect levels");
}
'''
with tempfile.TemporaryDirectory(prefix='sn-dac-runtime-') as directory:
    temp=Path(directory)
    shutil.copytree(root/'firmware/inc',temp/'inc')
    make_fake_sdk(temp)
    original=(root/'configurations/I2C-DAC/config.h').read_text()
    (temp/'contract.c').write_text(source)
    for variant in ('mixed-12bit','mixed-8-10bit','dac-only'):
        cfg=original
        if variant=='mixed-8-10bit':
            cfg=cfg.replace('SN_DAC_DAC7571','SN_DAC_DAC6571').replace('SN_DAC_AD5339','SN_DAC_AD5337').replace('1, 1024, 10','1, 128, 10')
        if variant=='dac-only':cfg=cfg.replace('#define I2C_IO_ENABLE 1','#define I2C_IO_ENABLE 0')
        (temp/'inc/config.h').write_text(cfg)
        subprocess.run([sys.argv[1],'-std=c11','-Wall','-Wextra','-Werror',
                        '-I'+str(temp),'-I'+str(temp/'inc'),'-I'+str(root/'firmware/modules/inc'),
                        '-I'+str(root/'firmware/modules'),str(temp/'contract.c'),'-o',str(temp/'contract')],check=True)
        subprocess.run([str(temp/'contract')],check=True)
