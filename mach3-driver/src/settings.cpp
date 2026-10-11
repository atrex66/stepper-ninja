#include "settings.h"
#include <cmath>
namespace sn {
bool parse_ipv4(const std::string &s, uint8_t octets[4]) {
    size_t at = 0;
    for (unsigned part = 0; part < 4; ++part) {
        unsigned value = 0, digits = 0;
        while (at < s.size() && s[at] >= '0' && s[at] <= '9') {
            value = value * 10 + unsigned(s[at++] - '0');
            if (++digits > 3 || value > 255) return false;
        }
        if (!digits) return false;
        octets[part] = static_cast<uint8_t>(value);
        if (part < 3 && (at >= s.size() || s[at++] != '.')) return false;
    }
    return at == s.size();
}
std::string validate_settings(const Settings &s) {
    uint8_t ip[4];
    if (!parse_ipv4(s.ip, ip) || ip[0] == 0 || ip[0] >= 224 ||
        (ip[0] == 255 && ip[1] == 255 && ip[2] == 255 && ip[3] == 255))
        return "Enter a unicast IPv4 address.";
    if (!s.port || s.port > 65535) return "UDP port must be 1..65535.";
    if (s.timeout_ms < 100 || s.timeout_ms > 2000) return "Reply timeout must be 100..2000 ms.";
    if (s.pwm_hz < 4000 || s.pwm_hz > 100000) return "PWM frequency must be 4000..100000 Hz.";
    if(s.watchdog_ms<100 || s.watchdog_ms>2000)return "Watchdog must be 100..2000 ms.";
    if(s.pulse_ns<1000 || s.pulse_ns>10000)return "STEP pulse must be 1000..10000 ns.";
    if(s.dir_setup_ns<1000 || s.dir_setup_ns>20000)return "DIR setup must be 1000..20000 ns.";
    if(s.input_invert>127 || s.output_invert>63 || s.direction_invert>7 || s.home_positive>7)
        return "Invalid input, output or axis bit mask.";
    if(!s.home_slow_percent || s.home_slow_percent>100)return "Slow home speed must be 1..100% of fast home speed.";
    if(!std::isfinite(s.home_backoff) || !std::isfinite(s.home_travel) || s.home_backoff<=0 ||
        s.home_travel<s.home_backoff || s.home_travel>1000000)return "Invalid home backoff or maximum travel (machine units).";
    return {};
}
}
