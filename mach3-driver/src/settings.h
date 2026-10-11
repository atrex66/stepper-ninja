#ifndef SN_MACH3_SETTINGS_H
#define SN_MACH3_SETTINGS_H
#include <stdint.h>
#include <string>
namespace sn {
struct Settings {
    std::string ip = "192.168.0.177";
    unsigned port = 8888;
    unsigned timeout_ms = 500;
    unsigned pwm_hz = 10000;
    unsigned watchdog_ms = 500;
    unsigned pulse_ns = 2500;
    unsigned dir_setup_ns = 5000;
    unsigned input_invert = 127;
    unsigned output_invert = 0;
    unsigned direction_invert = 0;
    unsigned home_positive = 0;
    unsigned home_slow_percent = 10;
    double home_backoff = 1.0;
    double home_travel = 1000.0;
};
bool parse_ipv4(const std::string &text, uint8_t octets[4]);
std::string validate_settings(const Settings &settings);
}
#endif
