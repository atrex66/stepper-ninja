#include <assert.h>
#include <stdio.h>
#include "i2c_dac_config.h"

int main(void)
{
    const sn_i2c_bus_config buses[] = {{0, 0, 1, 400000, 1000}};
    sn_i2c_dac_channel channels[] = {
        {SN_DAC_MCP4725, 0, 0x60, 0, 0, 10},
        {SN_DAC_DAC7571, 0, 0x4c, 0, 0, 10},
        {SN_DAC_AD5339, 0, 0x0c, 0, 0, 10},
        {SN_DAC_AD5339, 0, 0x0c, 1, 1024, 10}
    };
    assert(sn_i2c_dac_config_valid(buses, 1, channels, 4, NULL, 0));
    channels[3].channel=0;
    assert(!sn_i2c_dac_config_valid(buses, 1, channels, 4, NULL, 0));
    channels[3].channel=1; channels[3].type=SN_DAC_AD5337;
    assert(!sn_i2c_dac_config_valid(buses, 1, channels, 4, NULL, 0));
    channels[3].type=SN_DAC_AD5339; channels[3].safe_code=4096;
    assert(!sn_i2c_dac_config_valid(buses, 1, channels, 4, NULL, 0));
    channels[3].safe_code=0; channels[0].address=0;
    assert(!sn_i2c_dac_config_valid(buses, 1, channels, 4, NULL, 0));
    channels[0].address=0x60;
    for(unsigned type=0;type<=SN_DAC_AD5339;++type) {
        unsigned max = sn_i2c_dac_max(type);
        unsigned bits = sn_i2c_dac_bits(type);
        for(unsigned channel=0;channel<(type>=SN_DAC_AD5337?2u:1u);++channel)
            for(unsigned code=0;code<=max+1;++code) {
                uint8_t bytes[3]={0};
                unsigned len=sn_i2c_dac_encode(type,channel,code,bytes);
                unsigned c=code>max?max:code;
                if(type==SN_DAC_MCP4725) {
                    assert(len==3 && bytes[0]==0x40);
                    assert(bytes[1]==(c>>4) && bytes[2]==((c&15)<<4));
                } else {
                    unsigned offset=type>=SN_DAC_AD5337?1:0;
                    unsigned word=((unsigned)bytes[offset]<<8)|bytes[offset+1];
                    assert(len==offset+2);
                    // Independently decode the data alignment shown in the datasheets.
                    assert(((word&0xfff)>>(12-bits))==c);
                    assert((word&0xf000)==(offset?0x2000:0));
                    if(offset) assert(bytes[0]==(1u<<channel));
                }
            }
    }
    uint8_t bytes[3];
    assert(!sn_i2c_dac_encode(99,0,0,bytes));
    assert(!sn_i2c_dac_encode(SN_DAC_MCP4725,1,0,bytes));
    // ADC and broadcast addresses must never be mistaken for ordinary DAC writes.
    channels[1].address=0x48;
    assert(!sn_i2c_dac_config_valid(buses,1,channels,4,NULL,0));
    channels[1].address=0x4c;
    channels[2].type=SN_DAC_AD5338; channels[2].address=0x22;
    channels[3].type=SN_DAC_AD5338; channels[3].address=0x22;
    sn_i2c_device_config io[]={{SN_IO_TCA9555,0,0x22,0,0,0,0,0,10}};
    assert(sn_i2c_dac_config_valid(buses,1,channels,4,NULL,0));
    assert(!sn_i2c_dac_config_valid(buses,1,channels,4,io,1));
    puts("PASS: all seven DAC types, native code alignment, AD533x selectors/CLR/LDAC, clamping and configuration conflicts");
}
