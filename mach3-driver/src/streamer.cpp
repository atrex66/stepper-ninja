#include "streamer.h"
namespace sn {
void Streamer::reset() {
  accumulator_ = WaypointAccumulator();
  started_ = ended_ = false;
}
std::string Streamer::update(ControllerClient &client,
                             TrajectorySource &source) {
  if ((started_ || ended_) && client.report().mode == SN_IDLE &&
      !client.report().fill && !client.report().pending)
    reset();
  if (ended_ ||
      (client.report().mode != SN_IDLE && client.report().mode != SN_STREAM))
    return {};
  bool barrier = false;
  for (unsigned batch = 0; batch < 5 && client.report().fill < 100; ++batch) {
    unsigned count = source.available();
    if (count > 20)
      count = 20;
    if (count > client.report().free)
      count = client.report().free;
    if (!count)
      break;
    uint8_t payload[328] = {};
    sn_write_u32(payload, client.report().accepted + 1);
    WaypointAccumulator next = accumulator_;
    for (unsigned i = 0; i < count; ++i) {
      Waypoint move = source.peek(i);
      int32_t steps[3];
      if (move.type || move.delta[3] || move.delta[4] || move.delta[5] ||
          !next.convert(move.delta, steps))
        return "Unsupported waypoint type/axis or pulse rate. Queue was not "
               "consumed.";
      for (unsigned a = 0; a < 3; ++a)
        sn_write_u32(payload + 8 + i * 16 + a * 4,
                     static_cast<uint32_t>(steps[a]));
      sn_write_u32(payload + 8 + i * 16 + 12, move.line);
      if (move.stop) {
        count = i + 1;
        barrier = true;
        break;
      }
    }
    sn_write_u32(payload + 4, count);
    std::string error = client.command(SN_BATCH, payload, 8 + count * 16);
    if (!error.empty())
      return error;
    accumulator_ = next;
    source.consume(
        count); // ACK commits only ownership transfer, not execution.
    if (barrier)
      break;
  }
  bool ending = barrier || (!source.available() && source.ending());
  if (ending && !ended_ && (started_ || client.report().fill)) {
    std::string error = client.command(SN_END);
    if (!error.empty())
      return error;
    ended_ = true;
  }
  if (!started_ && client.report().fill &&
      (client.report().fill >= 40 || ending)) {
    std::string error = client.command(SN_START);
    if (!error.empty())
      return error;
    started_ = true;
  }
  return {};
}
} // namespace sn
