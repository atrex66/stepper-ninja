#!/usr/bin/env python3
"""Run I2C HAL pin creation and packet mapping against real LinuxCNC headers."""
from pathlib import Path
import argparse
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', default='cc')
parser.add_argument('--include', action='append', default=[])
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
harness = (root/'hal-driver/tests/driver_contract.c').read_text().split('int main(int argc, char **argv)')[0]
harness += r'''
int main(int argc, char **argv) {
    if (argc == 2) fail_at = atoi(argv[1]);
    ip_address = "127.0.0.1:0";
    int result = rtapi_app_main();
    if (fail_at >= 0) {
        assert(result == -ENOMEM && exit_count == 1);
        close(hal_data[0].sockfd);
        free(tx_buffer); free(rx_buffer); free(component_storage);
        return 0;
    }
    assert(result == 0);
    module_data_t *d = hal_data;
    transmission_pico_pc_t packet = {0};
    packet.inputs[0] = 1u << 22;
    packet.inputs[3] = 0x303;
    for (unsigned bit = 0; bit < 32; ++bit) {
        packet.inputs[2] = 1u << bit;
        receive_packet(&packet);
        assert(sn_get_bit(d->input[0]) == 1 && sn_get_bit(d->input[1]) == 0);
        for (unsigned n = 0; n < 2; ++n) {
            assert(sn_get_bit(d->i2c_online[n]) && sn_get_bit(d->i2c_valid[n]));
            for (unsigned p = 0; p < sn_i2c_io_width(&i2c_devices[n]); ++p)
                if (i2c_devices[n].direction & (1u << p)) {
                    assert(sn_get_bit(d->i2c_input[n][p]) == (bit == n*16+p));
                    assert(sn_get_bit(d->i2c_input_not[n][p]) == (bit != n*16+p));
                }
        }
    }
    sn_set_bit(d->output[0], 1);
    for (unsigned n = 0; n < 2; ++n)
        for (unsigned p = 0; p < sn_i2c_io_width(&i2c_devices[n]); ++p)
            if (!(i2c_devices[n].direction & (1u << p))) sn_set_bit(d->i2c_output[n][p], 0);
    for (unsigned n = 0; n < 2; ++n)
        for (unsigned p = 0; p < sn_i2c_io_width(&i2c_devices[n]); ++p)
            if (!(i2c_devices[n].direction & (1u << p))) {
                sn_set_bit(d->i2c_output[n][p], 1);
                bb_hal_process_send(d);
                assert(tx_buffer->outputs[0] == 1);
                assert(tx_buffer->outputs[1] == (1u << (n*16+p)));
                sn_set_bit(d->i2c_output[n][p], 0);
            }
    for (int i=0;i<pin_count;++i) {
        if (strstr(pins[i].name,".expander.")) {
            assert(pins[i].direction == (strstr(pins[i].name,".output.")?HAL_IN:HAL_OUT));
        }
    }
    d->watchdog_expired=1;
    udp_io_process_recv(d,1000000);
    for(unsigned n=0;n<2;++n) {
        assert(!sn_get_bit(d->i2c_online[n]) && !sn_get_bit(d->i2c_valid[n]));
    }
    printf("PASS: I2C HAL mapping, GPIO coexistence, directions, watchdog, %d pins\n",pin_count);
    rtapi_app_exit();
    free(tx_buffer); free(rx_buffer); free(component_storage);
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='sn-i2c-hal-') as directory:
    temp = Path(directory)
    shutil.copytree(root/'hal-driver',temp/'driver',symlinks=False,
                    ignore=shutil.ignore_patterns('build*','tests'))
    config = (root/'configurations/I2C-IO/config.h').read_text()
    (temp/'contract.c').write_text(harness)
    for variant in ('mcp08-tca9555','mcp17-tca9535'):
        cfg = config
        if variant == 'mcp17-tca9535':
            cfg=cfg.replace('SN_IO_MCP23008','SN_IO_MCP23017').replace('SN_IO_TCA9555','SN_IO_TCA9535')
        (temp/'driver/config.h').write_text(cfg)
        command = [args.compiler,'-std=gnu11','-DUSPACE','-DRTAPI','-D_GNU_SOURCE',
                   '-Drealtime','-D__MODULE__','-DSIM','-Werror=implicit-function-declaration',
                   '-Werror=incompatible-pointer-types','-DDRIVER_SOURCE="'+str(temp/'driver/stepgen-ninja.c')+'"',
                   '-I'+str(temp/'driver')]
        command += ['-I'+s for s in (args.include or ['/usr/include/linuxcnc'])]
        subprocess.run(command+[str(temp/'contract.c'),'-lm','-o',str(temp/'contract')],check=True)
        result=subprocess.run([str(temp/'contract')],check=True,text=True,capture_output=True)
        print(result.stdout.strip().splitlines()[-1])
        # Every exported pin creation must propagate allocation failure.
        count=int(result.stdout.strip().splitlines()[-1].split()[-2])
        for i in range(count):
            subprocess.run([str(temp/'contract'),str(i)],check=True,capture_output=True)
        print('PASS: all',count,'HAL pin allocation failure paths')
