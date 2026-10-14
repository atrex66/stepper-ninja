#include "probe_motion.h"
#include "../../common/protocol/mach3_control.h"
#include <cmath>
#include <cstring>
namespace sn {
std::string probe_packet(uint8_t out[32], const double goal[3],
                         const double scale[3], const int64_t position[3],
                         double feed_per_minute) {
  uint8_t packet[32] = {};
  double length2 = 0, dominant = 0;
  for (unsigned i = 0; i < 3; ++i) {
    double endpoint = goal[i] * scale[i];
    if (!std::isfinite(scale[i]) || scale[i] <= 0 || !std::isfinite(endpoint) ||
        std::fabs(endpoint) > 1e12)
      return "Invalid G31 endpoint/scale.";
    int64_t target = static_cast<int64_t>(std::round(endpoint));
    double delta = static_cast<double>(target) - position[i];
    if (std::fabs(delta) > 1e9)
      return "G31 travel exceeds firmware range.";
    sn_write_i64(packet + i * 8, target);
    dominant = std::fmax(dominant, std::fabs(delta));
    length2 += (delta / scale[i]) * (delta / scale[i]);
  }
  if (!dominant || !std::isfinite(feed_per_minute) || feed_per_minute <= 0)
    return "Invalid G31 feed or zero-length endpoint.";
  double velocity = feed_per_minute / 60.0 * dominant / std::sqrt(length2);
  if (!std::isfinite(velocity) || velocity < 1 || velocity > SN_MAX_STEP_RATE)
    return "G31 dominant-axis feed must be 1..200000 steps/s.";
  sn_write_u32(packet + 24, static_cast<uint32_t>(velocity));
  packet[28] = 4; // Fixed DI5 / GP26.
  std::memcpy(out, packet, sizeof(packet));
  return {};
}
std::string probe_work_position(double out[3], const int64_t captured[3],
                                const double scale[3],
                                const double origins[3]) {
  double next[3];
  for (unsigned i = 0; i < 3; ++i) {
    if (!std::isfinite(scale[i]) || scale[i] <= 0 || !std::isfinite(origins[i]))
      return "Invalid probe scale/origin.";
    next[i] = captured[i] / scale[i] - origins[i];
    if (!std::isfinite(next[i]))
      return "Invalid probe work coordinate.";
  }
  std::memcpy(out, next, sizeof(next));
  return {};
}
} // namespace sn
