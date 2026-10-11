#include "protocol/mach3_probe.h"
#include "profiles/mach3_3axis.h"
#include "settings.h"
#include <cassert>
#include <cstring>
#include <iostream>
int main() {
    assert(sn_crc32(reinterpret_cast<const uint8_t *>("123456789"), 9) == 0xcbf43926u);
    uint8_t req[SN_MACH3_PROBE_REQUEST_SIZE], reply[SN_MACH3_PROBE_REPLY_SIZE];
    sn_mach3_probe_request(req, 0x12345678);
    assert(req[8] == 0x78 && req[11] == 0x12);
    assert(sn_mach3_probe_reply(req, sizeof(req), reply));
    assert(sn_mach3_probe_validate(reply, sizeof(reply), 0x12345678));
    assert(!sn_mach3_probe_validate(reply, sizeof(reply), 0x12345679));
    assert(!sn_mach3_probe_validate(reply, sizeof(reply) - 1, 0x12345678));
    for (size_t bit = 0; bit < sizeof(req) * 8; ++bit) {
        req[bit / 8] ^= uint8_t(1u << (bit % 8));
        assert(!sn_mach3_probe_reply(req, sizeof(req), reply));
        req[bit / 8] ^= uint8_t(1u << (bit % 8));
    }
    assert(!sn_mach3_probe_reply(req, sizeof(req) - 1, reply));
    // A well-formed packet cannot smuggle any output/motion command in reserved bytes.
    req[12] = 1; sn_write_u32(req + 32, sn_crc32(req, 32));
    assert(!sn_mach3_probe_reply(req, sizeof(req), reply));
    sn_mach3_probe_request(req, 9); assert(sn_mach3_probe_reply(req, sizeof(req), reply));
    reply[16] = 4; sn_write_u32(reply + 24, sn_crc32(reply, 24));
    assert(!sn_mach3_probe_validate(reply, sizeof(reply), 9));
    sn::Settings settings; assert(sn::validate_settings(settings).empty());
    for (const char *ip : {"", "1.2.3", "1.2.3.4.5", "256.0.0.1", "1.2.3.-1", "1.2.3.4 ", "224.0.0.1", "0.0.0.0"}) {
        settings.ip = ip; assert(!sn::validate_settings(settings).empty());
    }
    settings = sn::Settings(); settings.port = 65536; assert(!sn::validate_settings(settings).empty());
    settings.port = 1; settings.timeout_ms = 99; assert(!sn::validate_settings(settings).empty());
    settings.timeout_ms = 500; settings.pwm_hz = 0; assert(!sn::validate_settings(settings).empty());
    // Physical pins are unique and do not overlap Ethernet or the onboard LED.
    const unsigned step[] = SN_MACH3_STEP_PINS, dir[] = SN_MACH3_DIR_PINS;
    const unsigned inputs[] = SN_MACH3_INPUT_PINS, outputs[] = SN_MACH3_OUTPUT_PINS;
    uint32_t used = 0;
    auto reserve = [&used](unsigned pin) {
        assert(pin < 29 && !(pin >= 16 && pin <= 21) && pin != 25);
        assert(!(used & (1u << pin))); used |= 1u << pin;
    };
    for (auto pin : step) reserve(pin);
    for (auto pin : dir) reserve(pin);
    for (auto pin : inputs) reserve(pin);
    for (auto pin : outputs) reserve(pin);
    reserve(SN_MACH3_PWM_PIN);
    uint32_t mask = 0; for (auto pin : inputs) mask |= 1u << pin;
    assert(mask == SN_MACH3_INPUT_GPIO_MASK);
    std::cout << "PASS: CRC, malformed requests, nonce/profile checks, settings and pin isolation\n";
}
