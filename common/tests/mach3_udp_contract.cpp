#include "udp_probe.h"
#include "protocol/mach3_probe.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cassert>
#include <thread>
#include <iostream>
int main() {
    for (int mode = 0; mode < 4; ++mode) {
        int server = socket(AF_INET, SOCK_DGRAM, 0); assert(server >= 0);
        sockaddr_in address = {}; address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        assert(bind(server, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0);
        socklen_t length = sizeof(address); assert(getsockname(server, reinterpret_cast<sockaddr *>(&address), &length) == 0);
        timeval timeout = {2, 0}; assert(setsockopt(server, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0);
        sn::Settings settings; settings.ip = "127.0.0.1"; settings.port = ntohs(address.sin_port); settings.timeout_ms = 100;
        std::thread device([server, mode]() {
            uint8_t req[SN_MACH3_PROBE_REQUEST_SIZE], reply[SN_MACH3_PROBE_REPLY_SIZE + 1];
            sockaddr_in client = {}; socklen_t size = sizeof(client);
            int n = recvfrom(server, req, sizeof(req), 0, reinterpret_cast<sockaddr *>(&client), &size);
            assert(n == sizeof(req)); assert(sn_mach3_probe_reply(req, n, reply));
            if (mode == 1) reply[12] ^= 1;
            if (mode != 2) {
                size_t count = SN_MACH3_PROBE_REPLY_SIZE + (mode == 3 ? 1 : 0);
                reply[SN_MACH3_PROBE_REPLY_SIZE] = 0;
                assert(sendto(server, reply, count, 0, reinterpret_cast<sockaddr *>(&client), size) == static_cast<int>(count));
            }
        });
        auto error = sn::probe_controller(settings); device.join(); close(server);
        assert((mode == 0) == error.empty());
    }
    std::cout << "PASS: UDP success, corrupt response, timeout and oversized response\n";
}
