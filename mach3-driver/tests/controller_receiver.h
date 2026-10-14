#pragma once
#include <afxwin.h>
#include <afxsock.h>
#include <atomic>
#include <chrono>
#include <thread>
#include "../../common/motion/controller.h"

class Receiver {
  SOCKET socket_;
  std::atomic<bool> running_;
  std::thread worker_;
public:
  unsigned port = 0;
  std::atomic<unsigned> raw{42}, owner{0};
  std::atomic<bool> legacy{false}, drop{false};
  std::atomic<unsigned> invert{127}, armed{0}, fault{0};
  std::atomic<unsigned> counts[32];
  Receiver() : socket_(INVALID_SOCKET), running_(true) {
    for (auto &count : counts) count = 0;
    socket_ = ::socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    sockaddr_in address = {};
    address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    if (socket_ == INVALID_SOCKET || bind(socket_,reinterpret_cast<sockaddr *>(&address),sizeof(address))) return;
    int size=sizeof(address);getsockname(socket_,reinterpret_cast<sockaddr *>(&address),&size);
    port=ntohs(address.sin_port);
    DWORD timeout=10;setsockopt(socket_,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<char *>(&timeout),sizeof(timeout));
    worker_=std::thread([this] {
      sn_controller controller;sn_controller_init(&controller);
      auto tick=std::chrono::steady_clock::now();
      while(running_) {
        auto now=std::chrono::steady_clock::now();
        while(tick<=now) {int32_t steps[3];sn_controller_tick(&controller,static_cast<uint8_t>(raw.load()),steps);tick+=std::chrono::milliseconds(1);}
        owner=controller.session;
        uint8_t request[SN_CONTROL_MAX_PACKET+1];sockaddr_in remote={};int remote_size=sizeof(remote);
        int n=recvfrom(socket_,reinterpret_cast<char *>(request),sizeof(request),0,reinterpret_cast<sockaddr *>(&remote),&remote_size);
        sn_packet packet;
        if(n<=0 || !sn_control_decode(request,n,&packet))continue;
        if(packet.command<32) ++counts[packet.command];
        uint32_t result=sn_controller_command(&controller,&packet,1);
        owner=controller.session;
        invert=controller.config.input_invert;armed=controller.armed;fault=controller.fault;
        uint8_t status[SN_CONTROL_STATUS_SIZE],response[SN_CONTROL_MAX_PACKET];
        sn_controller_status(&controller,status,result);
        if(legacy) {status[101]=0;status[102]=0;}
        if(drop && packet.command==SN_POLL)continue;
        size_t bytes=sn_control_encode(response,packet.command|0x8000u,packet.session,packet.sequence,status,sizeof(status));
        sendto(socket_,reinterpret_cast<char *>(response),static_cast<int>(bytes),0,reinterpret_cast<sockaddr *>(&remote),remote_size);
      }
    });
  }
  ~Receiver(){running_=false;if(worker_.joinable())worker_.join();if(socket_!=INVALID_SOCKET)closesocket(socket_);}
  bool readonly() const {
    for(unsigned i=0;i<32;++i)
      if(i!=SN_CLAIM && i!=SN_POLL && i!=SN_RELEASE && counts[i])return false;
    return true;
  }
};
