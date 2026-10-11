#ifndef SN_MACH3_STREAMER_H
#define SN_MACH3_STREAMER_H
#include "controller_client.h"
#include "waypoints.h"
namespace sn {
struct Waypoint {
  double delta[6] = {0, 0, 0, 0, 0, 0};
  unsigned type = 0;
  uint32_t line = 0;
  bool stop = false;
};
class TrajectorySource {
public:
  virtual ~TrajectorySource() {}
  virtual unsigned available() const = 0;
  virtual Waypoint peek(unsigned offset) const = 0;
  virtual void consume(unsigned count) = 0;
  virtual bool ending() const = 0;
};
/* Mach3 owns normal Feed Hold planning. The pump must continue transmitting its
 * braking records until the Stop/BufferEnding barrier, without purge/rewind. */
class Streamer {
public:
  void reset();
  std::string update(ControllerClient &client, TrajectorySource &source);

private:
  WaypointAccumulator accumulator_;
  bool started_ = false, ended_ = false;
};
} // namespace sn
#endif
