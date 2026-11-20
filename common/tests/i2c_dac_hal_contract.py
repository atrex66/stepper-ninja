#!/usr/bin/env python3
"""Exercise DAC HAL pins, scaling and packet mapping against real HAL headers."""
from pathlib import Path
import argparse
import shutil
import subprocess
import tempfile

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler',default='cc')
parser.add_argument('--include',action='append',default=[])
args=parser.parse_args()
root=Path(__file__).resolve().parents[2]
harness=(root/'hal-driver/tests/driver_contract.c').read_text().split('int main(int argc, char **argv)')[0]
harness+=r'''
int main(int argc,char **argv) {
    if(argc==2) fail_at=atoi(argv[1]);
    ip_address="127.0.0.1:0";
    int result=rtapi_app_main();
    if(fail_at>=0) {
        assert(result==-ENOMEM && exit_count==1);
        close(hal_data[0].sockfd);
        free(tx_buffer);free(rx_buffer);free(component_storage);return 0;
    }
    assert(result==0);
    module_data_t *d=hal_data;
    sn_set_bit(d->output[0],1);
    for(unsigned n=0;n<I2C_DAC_CHANNEL_COUNT;++n) {
        assert(!sn_get_bit(d->dac_enable[n]));
        bb_hal_process_send(d);assert(tx_buffer->analog_out[n]==dac_config[n].safe_code);
        sn_set_bit(d->dac_enable[n],1);
        sn_set_float(d->dac_scale[n],10);
        sn_set_float(d->dac_value[n],5);
        bb_hal_process_send(d);
        assert(tx_buffer->outputs[0]==1);
        assert(tx_buffer->analog_out[n]==((sn_i2c_dac_max(dac_config[n].type)+1)/2));
        assert(sn_get_u32(d->dac_raw[n])==tx_buffer->analog_out[n]);
        sn_set_float(d->dac_value[n],11);bb_hal_process_send(d);
        assert(tx_buffer->analog_out[n]==sn_i2c_dac_max(dac_config[n].type));
        sn_set_float(d->dac_value[n],-1);bb_hal_process_send(d);
        assert(tx_buffer->analog_out[n]==0);
        sn_set_float(d->dac_value[n],NAN);bb_hal_process_send(d);
        assert(tx_buffer->analog_out[n]==dac_config[n].safe_code);
        sn_set_float(d->dac_value[n],INFINITY);bb_hal_process_send(d);
        assert(tx_buffer->analog_out[n]==dac_config[n].safe_code);
        sn_set_float(d->dac_value[n],1);
        const double invalid_scales[]={0,-1,NAN,INFINITY};
        for(unsigned i=0;i<4;++i) {
            sn_set_float(d->dac_scale[n],invalid_scales[i]);bb_hal_process_send(d);
            assert(tx_buffer->analog_out[n]==dac_config[n].safe_code);
        }
        sn_set_bit(d->dac_enable[n],0);
    }
    transmission_pico_pc_t packet={0};
    packet.inputs[0]=1u<<22;
    for(unsigned n=0;n<I2C_DAC_CHANNEL_COUNT;++n) {
        packet.inputs[3]=(1u<<(16+n))|(1u<<(24+n));
        receive_packet(&packet);
        assert(sn_get_bit(d->input[0]));
        for(unsigned k=0;k<I2C_DAC_CHANNEL_COUNT;++k)
            assert(sn_get_bit(d->dac_online[k])==(k==n) && sn_get_bit(d->dac_valid[k])==(k==n));
    }
    for(int i=0;i<pin_count;++i) if(strstr(pins[i].name,".dac.")) {
        bool command=strstr(pins[i].name,".enable")||strstr(pins[i].name,".scale")||strstr(pins[i].name,".value");
        assert(pins[i].direction==(command?HAL_IN:HAL_OUT));
    }
    d->watchdog_expired=1;udp_io_process_recv(d,1000000);
    for(unsigned n=0;n<I2C_DAC_CHANNEL_COUNT;++n)
        assert(!sn_get_bit(d->dac_online[n]) && !sn_get_bit(d->dac_valid[n]));
    printf("PASS: DAC HAL scaling, finite guards, clipping, disable, status and packet mapping; %d pins\n",pin_count);
    rtapi_app_exit();free(tx_buffer);free(rx_buffer);free(component_storage);return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='sn-dac-hal-') as directory:
    temp=Path(directory)
    shutil.copytree(root/'hal-driver',temp/'driver',symlinks=False,ignore=shutil.ignore_patterns('build*','tests'))
    original=(root/'configurations/I2C-DAC/config.h').read_text()
    (temp/'contract.c').write_text(harness)
    for variant in ('mixed','dac-only','8-10bit'):
        cfg=original
        if variant=='dac-only':cfg=cfg.replace('#define I2C_IO_ENABLE 1','#define I2C_IO_ENABLE 0')
        if variant=='8-10bit':
            cfg=cfg.replace('SN_DAC_DAC7571','SN_DAC_DAC5571').replace('SN_DAC_AD5339','SN_DAC_AD5338')
            cfg=cfg.replace('1, 1024, 10','1, 512, 10')
        (temp/'driver/config.h').write_text(cfg)
        cmd=[args.compiler,'-std=gnu11','-DUSPACE','-DRTAPI','-D_GNU_SOURCE','-Drealtime','-D__MODULE__','-DSIM',
             '-Werror=implicit-function-declaration','-Werror=incompatible-pointer-types',
             '-DDRIVER_SOURCE="'+str(temp/'driver/stepgen-ninja.c')+'"','-I'+str(temp/'driver')]
        cmd+=['-I'+s for s in (args.include or ['/usr/include/linuxcnc'])]
        subprocess.run(cmd+[str(temp/'contract.c'),'-lm','-o',str(temp/'contract')],check=True,capture_output=True)
        result=subprocess.run([str(temp/'contract')],check=True,text=True,capture_output=True)
        last=result.stdout.strip().splitlines()[-1];print(last)
        count=int(last.split()[-2])
        for i in range(count):subprocess.run([str(temp/'contract'),str(i)],check=True,capture_output=True)
        print('PASS: all',count,'HAL pin allocation failure paths')
