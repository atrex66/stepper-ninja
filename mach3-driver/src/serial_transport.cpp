#include "serial_transport.h"
#include <chrono>
#include <algorithm>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#endif
namespace sn {
void SerialTransport::close() {
  if (connected()) {
#ifdef _WIN32
    CloseHandle(reinterpret_cast<HANDLE>(handle_));
#else
    ::close(static_cast<int>(handle_));
#endif
  }
  handle_ = -1;
  decoder_ = sn_usb_decoder();
  pending_at_ = pending_size_ = 0;
}
std::string SerialTransport::open(const std::string &port) {
  close();
#ifdef _WIN32
  std::string path = "\\\\.\\" + port;
  HANDLE h = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                        nullptr, OPEN_EXISTING, 0, nullptr);
  if (h == INVALID_HANDLE_VALUE) return "Cannot open USB COM port (busy or unavailable).";
  handle_ = reinterpret_cast<intptr_t>(h);
  DCB dcb = {};
  dcb.DCBlength = sizeof(dcb);
  if (!GetCommState(h, &dcb)) { close(); return "Cannot read COM port settings."; }
  dcb.BaudRate = CBR_115200;
  dcb.ByteSize = 8; dcb.Parity = NOPARITY; dcb.StopBits = ONESTOPBIT;
  dcb.fBinary = TRUE; dcb.fParity = FALSE;
  dcb.fOutxCtsFlow = dcb.fOutxDsrFlow = FALSE;
  dcb.fDtrControl = DTR_CONTROL_ENABLE;
  dcb.fRtsControl = RTS_CONTROL_DISABLE;
  dcb.fDsrSensitivity = dcb.fOutX = dcb.fInX = FALSE;
  dcb.fErrorChar = dcb.fNull = dcb.fAbortOnError = FALSE;
  if (!SetCommState(h, &dcb)) {
    const DWORD code = GetLastError();
    close(); return "SetCommState failed (Win32 " + std::to_string(code) + ").";
  }
  if (!PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR)) {
    const DWORD code = GetLastError();
    close(); return "PurgeComm failed (Win32 " + std::to_string(code) + ").";
  }
#else
  int fd = ::open(port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd < 0) return "Cannot open USB serial port.";
  handle_ = fd;
  termios t;
  if (tcgetattr(fd, &t)) { close(); return "Cannot read serial settings."; }
  cfmakeraw(&t);
  t.c_cflag |= CLOCAL | CREAD;
  t.c_cflag &= ~CRTSCTS;
  cfsetispeed(&t, B115200); cfsetospeed(&t, B115200);
  if (tcsetattr(fd, TCSANOW, &t)) { close(); return "Cannot configure serial port."; }
  tcflush(fd, TCIOFLUSH);
#endif
  return {};
}
bool SerialTransport::send(const uint8_t *data, size_t n, unsigned ms) {
  uint8_t frame[SN_USB_MAX_FRAME];
  size_t length = sn_usb_encode(frame, data, n), at = 0;
  if (!connected() || !length) return false;
  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
  while (at < length && std::chrono::steady_clock::now() < deadline) {
#ifdef _WIN32
    COMMTIMEOUTS timeouts = {};
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.WriteTotalTimeoutConstant = ms;
    if (!SetCommTimeouts(reinterpret_cast<HANDLE>(handle_), &timeouts)) return false;
    DWORD written = 0;
    if (!WriteFile(reinterpret_cast<HANDLE>(handle_), frame + at,
                   static_cast<DWORD>(length - at), &written, nullptr)) return false;
    at += written;
#else
    pollfd p = {static_cast<int>(handle_), POLLOUT, 0};
    auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
    if (poll(&p, 1, static_cast<int>(left > 0 ? left : 0)) <= 0) return false;
    ssize_t written = ::write(p.fd, frame + at, length - at);
    if (written < 0) { if (errno == EAGAIN || errno == EINTR) continue; return false; }
    at += static_cast<size_t>(written);
#endif
  }
  return at == length;
}
int SerialTransport::receive(uint8_t *data, size_t capacity, unsigned ms) {
  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
  while (connected() && std::chrono::steady_clock::now() < deadline) {
    if (pending_at_ == pending_size_) {
      pending_at_ = pending_size_ = 0;
      auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
      if (left <= 0) return 0;
#ifdef _WIN32
      COMMTIMEOUTS t = {};
      // Return buffered bytes immediately, otherwise wait for the first byte.
      t.ReadIntervalTimeout = MAXDWORD;
      t.ReadTotalTimeoutMultiplier = MAXDWORD;
      t.ReadTotalTimeoutConstant = static_cast<DWORD>(left);
      t.WriteTotalTimeoutConstant = ms;
      if (!SetCommTimeouts(reinterpret_cast<HANDLE>(handle_), &t)) return -1;
      DWORD count = 0;
      if (!ReadFile(reinterpret_cast<HANDLE>(handle_), pending_, sizeof(pending_), &count, nullptr)) return -1;
      pending_size_ = count;
#else
      pollfd p = {static_cast<int>(handle_), POLLIN, 0};
      int ready = poll(&p, 1, static_cast<int>(left));
      if (!ready) return 0;
      if (ready < 0) { if (errno == EINTR) continue; return -1; }
      ssize_t count = ::read(p.fd, pending_, sizeof(pending_));
      if (count < 0) { if (errno == EAGAIN || errno == EINTR) continue; return -1; }
      if (!count) return -1;
      pending_size_ = static_cast<size_t>(count);
#endif
      if (!pending_size_) continue;
    }
    uint8_t byte = pending_[pending_at_++];
    sn_packet packet;
    size_t size = decoder_.size;
    if (sn_usb_feed(&decoder_, byte, &packet)) {
      if (size > capacity) return -1;
      std::copy(decoder_.data, decoder_.data + size, data);
      return static_cast<int>(size);
    }
  }
  return 0;
}
}
