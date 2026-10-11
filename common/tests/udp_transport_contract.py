#!/usr/bin/env python3
"""Exercise the real WIZ UDP wrappers with a deterministic register/FIFO fake."""
import pathlib
import subprocess
import sys
import tempfile
root = pathlib.Path(__file__).resolve().parents[2]
header = r'''
#include <stdint.h>
#define __time_critical_func(name) name
#define Sn_CR_SEND 1
#define Sn_CR_RECV 2
#define Sn_IR_SENDOK 1
#define Sn_IR_TIMEOUT 2
#define SOCK_CLOSED 0
#define SOCKERR_TIMEOUT -13
#define SOCKERR_SOCKCLOSED -7
void setSn_DIPR(uint8_t, uint8_t *);
void setSn_DPORT(uint8_t, uint16_t);
void wiz_send_data(uint8_t, uint8_t *, uint16_t);
void wiz_recv_data(uint8_t, uint8_t *, uint16_t);
void setSn_CR(uint8_t, uint8_t);
uint8_t getSn_CR(uint8_t);
uint8_t getSn_IR(uint8_t);
void setSn_IR(uint8_t, uint8_t);
uint8_t getSn_SR(uint8_t);
uint16_t getSn_RX_RSR(uint8_t);
'''
source = r'''
#include "udp_transport.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
uint8_t first_send = 1;
static uint8_t fifo[2048], dest[4], interrupt=1;
static unsigned rd, wr;
static uint16_t destination_port;
void setSn_DIPR(uint8_t s,uint8_t *ip){(void)s;memcpy(dest,ip,4);}
void setSn_DPORT(uint8_t s,uint16_t p){(void)s;destination_port=p;}
void wiz_send_data(uint8_t s,uint8_t *p,uint16_t n){(void)s;(void)p;(void)n;}
void wiz_recv_data(uint8_t s,uint8_t *p,uint16_t n){(void)s;assert(rd+n<=wr);memcpy(p,fifo+rd,n);rd+=n;}
void setSn_CR(uint8_t s,uint8_t v){(void)s;(void)v;}
uint8_t getSn_CR(uint8_t s){(void)s;return 0;}
uint8_t getSn_IR(uint8_t s){(void)s;return interrupt;}
void setSn_IR(uint8_t s,uint8_t v){(void)s;(void)v;}
uint8_t getSn_SR(uint8_t s){(void)s;return 0;}
uint16_t getSn_RX_RSR(uint8_t s){(void)s;return wr-rd;}
static void datagram(unsigned n,uint8_t value){
    uint8_t h[8]={192,168,1,20,0x12,0x34,(uint8_t)(n>>8),(uint8_t)n};
    memcpy(fifo+wr,h,8);wr+=8;memset(fifo+wr,value,n);wr+=n;
}
int main(void){
    uint8_t p[512];sn_udp_peer peer;
    assert(sn_udp_receive(0,p,sizeof(p),&peer)==0);
    datagram(700,0x55);datagram(10,0xab);
    assert(sn_udp_receive(0,p,sizeof(p),&peer)==-1);
    assert(sn_udp_receive(0,p,sizeof(p),&peer)==10);
    assert(peer.ip[3]==20&&peer.port==0x1234&&p[9]==0xab&&rd==wr);
    assert(sn_udp_send(0,p,10,&peer)==10&&destination_port==0x1234&&dest[3]==20);
    peer.ip[3]=21;peer.port=5678;
    assert(sn_udp_send(0,p,10,&peer)==10&&destination_port==5678&&dest[3]==21);
    /* Legacy LinuxCNC retains its original sticky destination. */
    uint8_t other[4]={10,0,0,5};
    assert(_sendto(0,p,10,other,9000)==10&&destination_port==5678&&dest[3]==21);
    first_send=1;assert(_sendto(0,p,10,other,9000)==10&&dest[3]==5&&destination_port==9000);
    datagram(36,0x42);uint16_t port;
    assert(_recvfrom(0,p,36,other,&port)==36&&p[35]==0x42&&port==0x1234);
    interrupt=2;assert(sn_udp_send(0,p,10,&peer)==-13);
    puts("PASS: oversize drain, next datagram, peer routing, legacy routing and timeout");
}
'''
with tempfile.TemporaryDirectory(prefix="sn-udp-transport-") as directory:
    path = pathlib.Path(directory)
    (path / "fake.h").write_text(header)
    for name in ("pico.h", "wizchip_conf.h", "socket.h"):
        (path / name).write_text('#include "fake.h"\n')
    (path / "test.c").write_text(source)
    subprocess.run([sys.argv[1], "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-I" + str(path), "-I" + str(root / "firmware/modules/inc"),
                    str(root / "firmware/modules/udp_transport.c"), str(path / "test.c"),
                    "-o", str(path / "test")], check=True)
    subprocess.run([str(path / "test")], check=True)
