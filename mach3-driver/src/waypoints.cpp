#include "waypoints.h"
#include <cmath>
namespace sn {
bool WaypointAccumulator::convert(const double delta[3], int32_t steps[3]) {
  double remainder[3];
  for (unsigned i = 0; i < 3; ++i) {
    double total = fraction[i] + delta[i];
    if (!std::isfinite(total) || std::fabs(total) > SN_MAX_STEPS_TICK)
      return false;
    double rounded = std::round(total);
    if (std::fabs(rounded) > SN_MAX_STEPS_TICK)
      return false;
    steps[i] = static_cast<int32_t>(rounded);
    remainder[i] = total - rounded;
  }
  for (unsigned i = 0; i < 3; ++i)
    fraction[i] = remainder[i];
  return true;
}
} // namespace sn
