#ifndef SN_MACH3_PROBE_MOTION_H
#define SN_MACH3_PROBE_MOTION_H
#include <stdint.h>
#include <string>
namespace sn {
std::string probe_packet(uint8_t out[32], const double goal[3],
                         const double scale[3], const int64_t position[3],
                         double feed_per_minute);
std::string probe_work_position(double out[3], const int64_t captured[3],
                                const double scale[3], const double origins[3]);
} // namespace sn
#endif
