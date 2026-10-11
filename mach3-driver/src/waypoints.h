#ifndef SN_WAYPOINTS_H
#define SN_WAYPOINTS_H
#include "controller_client.h"
namespace sn {
/* Mach3 EX_VMS increments in motor steps, as in the official ncPod sample.
 * State is committed by the adapter only after the complete batch is ACKed. */
struct WaypointAccumulator {
  double fraction[3] = {0, 0, 0};
  bool convert(const double delta[3], int32_t steps[3]);
};
} // namespace sn
#endif
