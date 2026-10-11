#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/select.h>
#endif
#include "udp_probe.h"
#include "../../common/protocol/mach3_probe.h"
#include <chrono>
namespace sn {
std::string probe_controller(const Settings &settings) {
    std::string error = validate_settings(settings);
    if (!error.empty()) return error;
#ifdef _WIN32
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data)) return "Winsock initialization failed.";
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    const SOCKET invalid = INVALID_SOCKET;
#else
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    const int invalid = -1;
#endif
    if (sock == invalid) {
#ifdef _WIN32
        WSACleanup();
#endif
        return "Cannot create UDP socket.";
    }
    uint8_t octets[4]; parse_ipv4(settings.ip, octets);
    sockaddr_in remote = {};
    remote.sin_family = AF_INET; remote.sin_port = htons(static_cast<uint16_t>(settings.port));
    /* memcpy avoids host-endian assumptions. */
    unsigned char *address = reinterpret_cast<unsigned char *>(&remote.sin_addr.s_addr);
    for (unsigned i = 0; i < 4; ++i) address[i] = octets[i];
    uint32_t nonce = static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    uint8_t request[SN_MACH3_PROBE_REQUEST_SIZE]; sn_mach3_probe_request(request, nonce);
    if (connect(sock, reinterpret_cast<sockaddr *>(&remote), sizeof(remote))) {
        error = "Cannot connect UDP socket.";
    } else if (send(sock, reinterpret_cast<const char *>(request), sizeof(request), 0) != static_cast<int>(sizeof(request))) {
        error = "Cannot send controller probe.";
    } else {
        fd_set ready; FD_ZERO(&ready); FD_SET(sock, &ready);
        timeval timeout = {};
        timeout.tv_sec = settings.timeout_ms / 1000;
        timeout.tv_usec = (settings.timeout_ms % 1000) * 1000;
        int result = select(static_cast<int>(sock) + 1, &ready, nullptr, nullptr, &timeout);
        if (result == 0) error = "No response. Check IP, port and the Mach3 firmware.";
        else if (result < 0) error = "UDP receive failed.";
        else {
            uint8_t reply[SN_MACH3_PROBE_REPLY_SIZE + 1];
            int size = recv(sock, reinterpret_cast<char *>(reply), sizeof(reply), 0);
            if (size < 0 || !sn_mach3_probe_validate(reply, static_cast<size_t>(size), nonce))
                error = "Incompatible or invalid response (profile, version, nonce or CRC).";
        }
    }
#ifdef _WIN32
    closesocket(sock); WSACleanup();
#else
    close(sock);
#endif
    return error;
}
}
