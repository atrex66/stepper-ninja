#ifdef _WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include "controller_client.h"
#include <chrono>
#include <cstring>
namespace sn {
ControllerClient::ControllerClient() {}
ControllerClient::~ControllerClient() { close(); }
bool ControllerClient::connected() const { return socket_ != -1 || serial_.connected(); }
void ControllerClient::close() {
  serial_.close();
  if (socket_ != -1) {
#ifdef _WIN32
    closesocket(static_cast<SOCKET>(socket_));
#else
    ::close(static_cast<int>(socket_));
#endif
  }
  socket_ = -1;
  session_ = sequence_ = 0;
  report_ = Report();
#ifdef _WIN32
  if (winsock_)
    WSACleanup();
#endif
  winsock_ = false;
}
std::string ControllerClient::open(const Settings &s) {
  close();
  std::string error = validate_settings(s);
  if (!error.empty())
    return error;
  timeout_ms_ = s.timeout_ms;
  if (s.usb) {
    error = serial_.open(s.serial_port);
    if (!error.empty()) return error;
    session_ = static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count()) | 1u;
    error = command(SN_CLAIM);
    if (!error.empty()) close();
    return error;
  }
#ifdef _WIN32
  WSADATA data;
  if (WSAStartup(MAKEWORD(2, 2), &data))
    return "Winsock initialization failed.";
  winsock_ = true;
  SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock == INVALID_SOCKET) {
    close();
    return "Cannot create controller socket.";
  }
#else
  int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock < 0)
    return "Cannot create controller socket.";
#endif
  socket_ = static_cast<intptr_t>(sock);
  timeout_ms_ = s.timeout_ms < 100 ? 100 : s.timeout_ms;
  sockaddr_in remote = {};
  remote.sin_family = AF_INET;
  remote.sin_port = htons(static_cast<uint16_t>(s.port));
  uint8_t octets[4];
  parse_ipv4(s.ip, octets);
  std::memcpy(&remote.sin_addr.s_addr, octets, 4);
  if (connect(sock, reinterpret_cast<sockaddr *>(&remote), sizeof(remote))) {
    close();
    return "UDP connect failed.";
  }
  session_ = static_cast<uint32_t>(
                 std::chrono::steady_clock::now().time_since_epoch().count()) |
             1u;
  error = command(SN_CLAIM);
  if (!error.empty())
    close();
  return error;
}
std::string ControllerClient::command(uint16_t type, const uint8_t *payload,
                                      uint16_t size) {
  if (!connected())
    return "Controller is disconnected.";
  uint8_t request[SN_CONTROL_MAX_PACKET], response[SN_CONTROL_MAX_PACKET + 1];
  uint32_t seq = sequence_ + 1;
  size_t length =
      sn_control_encode(request, type, session_, seq, payload, size);
  if (!length)
    return "Invalid command payload size.";
  auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms_);
  for (unsigned attempt = 0;
       attempt < 3 && std::chrono::steady_clock::now() < deadline; ++attempt) {
#ifdef _WIN32
    SOCKET sock = static_cast<SOCKET>(socket_);
#else
    int sock = static_cast<int>(socket_);
#endif
    if (serial_.connected() ? !serial_.send(request, length, timeout_ms_ / 3) :
        send(sock, reinterpret_cast<const char *>(request),
             static_cast<int>(length), 0) != static_cast<int>(length))
      break;
    auto retry_end = std::chrono::steady_clock::now() +
                     std::chrono::milliseconds(timeout_ms_ / 3);
    if (retry_end > deadline)
      retry_end = deadline;
    while (std::chrono::steady_clock::now() < retry_end) {
      auto remain = std::chrono::duration_cast<std::chrono::microseconds>(
                        retry_end - std::chrono::steady_clock::now())
                        .count();
      if (remain <= 0)
        break;
      fd_set ready;
      FD_ZERO(&ready);
      if (!serial_.connected()) FD_SET(sock, &ready);
      timeval wait = {};
      wait.tv_sec = static_cast<long>(remain / 1000000);
      wait.tv_usec = static_cast<long>(remain % 1000000);
      int received = serial_.connected() ? 1 :
          select(static_cast<int>(sock) + 1, &ready, nullptr, nullptr, &wait);
      if (received <= 0)
        break;
      int n =
          serial_.connected() ? serial_.receive(response, sizeof(response),
              static_cast<unsigned>((remain + 999) / 1000)) :
          recv(sock, reinterpret_cast<char *>(response), sizeof(response), 0);
      sn_packet packet;
      if (n < 0 ||
          !sn_control_decode(response, static_cast<size_t>(n), &packet) ||
          packet.command != (type | 0x8000u) || packet.session != session_ ||
          packet.sequence != seq || packet.size != SN_CONTROL_STATUS_SIZE)
        continue;
      const uint8_t *p = packet.payload;
      Report next;
      next.result = sn_read_u32(p);
      next.acknowledged = sn_read_u32(p + 4);
      next.accepted = sn_read_u32(p + 8);
      next.executed = sn_read_u32(p + 12);
      next.line = sn_read_u32(p + 16);
      next.epoch = sn_read_u32(p + 20);
      next.fill = p[28] | p[29] << 8;
      next.free = p[30] | p[31] << 8;
      next.mode = p[32];
      next.fault = p[33];
      next.armed = p[34];
      next.configured = p[35];
      next.inputs = p[36];
      next.outputs = p[37];
      next.homed = p[38];
      next.home_phase = p[39];
      next.end = p[42] != 0;
      next.pending = p[43] != 0;
      for (unsigned i = 0; i < 3; ++i) {
        next.position[i] = sn_read_i64(p + 48 + i * 8);
        next.probe_position[i] = sn_read_i64(p + 72 + i * 8);
      }
      next.probe_id = sn_read_u32(p + 96);
      next.probe_state = p[100];
      report_ = next;
      if (next.acknowledged != seq && type != SN_CLAIM) {
        close();
        return "Session or command sequence rejected.";
      }
      sequence_ = seq;
      if (next.result != SN_OK)
        return "Controller rejected command (code " +
               std::to_string(next.result) + ").";
      return {};
    }
  }
  close();
  return "Controller response timeout. Firmware watchdog will stop the "
         "machine.";
}
} // namespace sn
