#!/usr/bin/env python3
"""Execute the production I2C driver with register-level fake SDK devices."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from i2c_fake_sdk import make_fake_sdk

root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix="sn-i2c-contract-") as directory:
    temp = Path(directory)
    shutil.copytree(root / "firmware/inc", temp / "inc")
    shutil.copyfile(root / "configurations/I2C-IO/config.h", temp / "inc/config.h")
    make_fake_sdk(temp)
    source = r'''
#include <assert.h>
#include <string.h>
#include "i2c_io.c"
i2c_inst_t fake_ports[2] = {{0}, {1}};
static uint8_t regs[2][256], cursor[2];
static uint64_t now;
static unsigned init_count, transactions, fail_bus = 99, short_bus = 99;
static unsigned latch_seen[2], direction_enabled[2];
uint64_t time_us_64(void) { return now; }
void gpio_set_function(unsigned pin, unsigned f) { assert(pin < 30 && f == GPIO_FUNC_I2C); }
void gpio_pull_up(unsigned pin) { assert(pin < 30); }
unsigned i2c_init(i2c_inst_t *p, unsigned hz) {
    assert(p->id < 2 && hz == 400000); ++init_count; return hz;
}
int i2c_write_timeout_us(i2c_inst_t *p, uint8_t addr, const uint8_t *data,
                         size_t len, bool nostop, unsigned timeout) {
    unsigned n = p->id;
    assert(addr == 0x20 && timeout == 1000); ++transactions;
    if (n == fail_bus) return -1;
    if (len == 1) { assert(nostop); cursor[n] = data[0]; return 1; }
    assert(len == 2 && !nostop);
    unsigned output = output_reg(n), dir = direction_reg(n);
    if (data[0] == output) latch_seen[n] = 1;
    if ((data[0] == dir || (sn_i2c_io_width(&devices[n]) == 16 && data[0] == dir+1)) && data[1] != 0xff) {
        assert(latch_seen[n]); direction_enabled[n] = 1;
    }
    regs[n][data[0]] = data[1]; return 2;
}
int i2c_read_timeout_us(i2c_inst_t *p, uint8_t addr, uint8_t *data,
                        size_t len, bool nostop, unsigned timeout) {
    assert(addr == 0x20 && len == 1 && !nostop && timeout == 1000); ++transactions;
    if (p->id == fail_bus) return -1;
    if (p->id == short_bus) return 0;
    *data = regs[p->id][cursor[p->id]]; return 1;
}
static uint16_t output(unsigned n) {
    return regs[n][output_reg(n)] | ((uint16_t)regs[n][output_reg(n)+1] << 8);
}
int main(void) {
    sn_i2c_bus_config b[] = I2C_IO_BUSES;
    sn_i2c_device_config d[] = I2C_IO_DEVICES;
    assert(sn_i2c_io_config_valid(b,2,d,2));
    b[1].controller=0; assert(!sn_i2c_io_config_valid(b,2,d,2)); b[1]=buses[1];
    d[1].bus=0; assert(!sn_i2c_io_config_valid(b,2,d,2)); d[1]=devices[1];
    d[1].pullup=1; assert(!sn_i2c_io_config_valid(b,2,d,2)); d[1]=devices[1];
    d[0].direction=0xffff; assert(!sn_i2c_io_config_valid(b,2,d,2)); d[0]=devices[0];
    d[0].type=SN_IO_MCP23017; d[0].direction=0xffff;
    assert(!sn_i2c_io_config_valid(b,2,d,2));
    sn_i2c_io_setup();
#ifdef CONFLICT
    assert(!enabled && init_count == 0);
    puts("PASS: GPIO conflict leaves buses untouched"); return 0;
#endif
    assert(enabled && init_count == 2);
    uint32_t input, status;
    sn_i2c_io_poll(false);
    assert(direction_enabled[0] && direction_enabled[1]);
    for(unsigned n=0;n<2;++n) {
        unsigned mask=sn_i2c_io_width(&devices[n])==8?0xff:0xffff;
        assert((output(n)&mask)==physical_outputs(n,devices[n].safe_outputs));
        regs[n][input_reg(n)] = 0xa5;
        if(sn_i2c_io_width(&devices[n])==16) regs[n][input_reg(n)+1]=0x5a;
    }
    sn_i2c_io_packet(0x550000a0, &input, &status);
    assert(status==0x303);
    unsigned count=transactions;
    for(unsigned i=0;i<100;++i) sn_i2c_io_packet(0x550000a0, &input, &status);
    assert(transactions==count); // packet path must never access I2C
    sn_i2c_io_poll(true); assert(transactions==count); // interval respected
    now=10000; sn_i2c_io_poll(true);
    sn_i2c_io_packet(0x550000a0,&input,&status);
    assert(status==0x303);
    for(unsigned n=0;n<2;++n) {
        uint16_t expected=((sn_i2c_io_width(&devices[n])==8?0xa5:0x5aa5)^devices[n].input_invert)&devices[n].direction;
        assert(((input>>(16*n))&0xffff)==expected);
        unsigned mask=sn_i2c_io_width(&devices[n])==8?0xff:0xffff;
        assert((output(n)&mask)==physical_outputs(n,n?0x5500:0x00a0));
    }
    uint32_t old=input;
    fail_bus=0; now=20000; sn_i2c_io_poll(true);
    sn_i2c_io_packet(0,&input,&status);
    assert(status==0x202 && input==old); // held sample explicitly invalid
    fail_bus=99; short_bus=1; now=30000; sn_i2c_io_poll(true);
    sn_i2c_io_packet(0,&input,&status); assert(status==0x101);
    short_bus=99; now=40000; sn_i2c_io_poll(true);
    sn_i2c_io_packet(0,&input,&status); assert(status==0x303);
    // Disconnect forces safe outputs immediately, even before the next poll.
    sn_i2c_io_poll(false);
    for(unsigned n=0;n<2;++n) {
        unsigned mask=sn_i2c_io_width(&devices[n])==8?0xff:0xffff;
        assert((output(n)&mask)==physical_outputs(n,devices[n].safe_outputs));
    }
    puts("PASS: register maps, safe latch before directions, inversion, packet snapshot, polling, NAK/short read, recovery, disconnect");
}
'''
    (temp / "contract.c").write_text(source)
    original = (temp / "inc/config.h").read_text()
    # Every existing preset remains disabled, including the standalone legacy
    # printer-port header which does not include footer.h (undefined #if is 0).
    (temp / "defaults.c").write_text('#include "config.h"\n#if I2C_IO_ENABLE || I2C_IO_BUS_COUNT || I2C_IO_DEVICE_COUNT || I2C_DAC_ENABLE || I2C_DAC_CHANNEL_COUNT\n#error Legacy preset enabled I2C IO\n#endif\nint main(void){return 0;}\n')
    for config in [root / "firmware/inc/config.h", *root.glob("configurations/**/*.h")]:
        if any(part in {"I2C-IO", "I2C-DAC"} for part in config.parts): continue
        shutil.copyfile(config, temp / "inc/config.h")
        subprocess.run([sys.argv[1], "-std=c11", "-I"+str(temp/"inc"),
                        "-c", str(temp/"defaults.c"), "-o", str(temp/"defaults.o")], check=True)
    print("PASS: all six existing configurations leave I2C IO and DAC disabled")
    for name, cfg, defines in [
        ("fixed board", original.replace("#define breakout_board 0", "#define breakout_board 1"), []),
        ("Mach3", original, ["-DSN_HOST_MACH3=1"]),
        ("count mismatch", original.replace("#define I2C_IO_DEVICE_COUNT 2", "#define I2C_IO_DEVICE_COUNT 1"), []),
        ("native output overlap", original.replace("#define out_pins {GP24}", "#define out_pins {"+",".join("24" for _ in range(33))+"}"), [])]:
        (temp / "inc/config.h").write_text(cfg)
        result = subprocess.run([sys.argv[1], "-std=c11", *defines,
                                "-I"+str(temp), "-I"+str(temp/"inc"),
                                "-I"+str(root/"firmware/modules/inc"), "-I"+str(root/"firmware/modules"),
                                "-c", str(temp/"contract.c"), "-o", str(temp/"invalid.o")], capture_output=True)
        assert result.returncode != 0, name
    print("PASS: fixed-board/Mach3 mixing, wrong counts and output overlap rejected")
    for variant in ("mcp08-tca9555", "mcp17-tca9535", "gpio-conflict"):
        config = original
        defines = []
        if variant == "mcp17-tca9535":
            config = config.replace("SN_IO_MCP23008", "SN_IO_MCP23017").replace("SN_IO_TCA9555", "SN_IO_TCA9535")
        if variant == "gpio-conflict":
            config = config.replace("{0, GP00, GP01", "{0, GP16, GP17")
            defines = ["-DCONFLICT"]
        (temp / "inc/config.h").write_text(config)
        subprocess.run([sys.argv[1], "-std=c11", "-Wall", "-Wextra", "-Werror", *defines,
                        "-I"+str(temp), "-I"+str(temp/"inc"),
                        "-I"+str(root/"firmware/modules/inc"), "-I"+str(root/"firmware/modules"),
                        str(temp/"contract.c"), "-o", str(temp/"contract")], check=True)
        subprocess.run([str(temp/"contract")], check=True)
