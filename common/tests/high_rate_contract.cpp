#include "controller_test_rig.h"
#include "motion/step_timing.h"
#include "waypoints.h"
#include "probe_motion.h"
#include <cassert>
#include <cstdio>
#include <cstring>

static void rate_stream(unsigned rate, unsigned pulse, const int frames[3], int total) {
  Rig rig;assert(rig.send(SN_CLAIM)==SN_OK);
  sn_machine_config config;sn_config_defaults(&config);config.pulse_ns=pulse;
  for(auto &axis:config.axis){axis.max_velocity=rate;axis.acceleration=250000;}
  uint8_t bytes[128];sn_config_encode(bytes,&config);
  assert(rig.send(SN_CONFIG,bytes,sizeof(bytes))==SN_OK);
  assert(rig.send(SN_ARM)==SN_OK);
  uint8_t batch[56]={};sn_write_u32(batch,1);sn_write_u32(batch+4,3);
  for(unsigned n=0;n<3;++n)for(unsigned a=0;a<3;++a)
    sn_write_u32(batch+8+n*16+a*4,static_cast<uint32_t>(a==1?-frames[n]:frames[n]));
  // Reject the entire batch on a single 201-step record; then accept the valid batch.
  uint8_t bad[56];std::memcpy(bad,batch,sizeof(bad));sn_write_u32(bad+8,201);
  assert(rig.send(SN_BATCH,bad,sizeof(bad))==SN_RANGE && rig.c.fill==0);
  assert(rig.send(SN_BATCH,batch,sizeof(batch))==SN_OK);
  assert(rig.send(SN_END)==SN_OK && rig.send(SN_START)==SN_OK);
  for(unsigned n=0;n<4;++n)rig.step();
  assert(rig.c.position[0]==total && rig.c.position[1]==-total && rig.c.position[2]==total);
  assert(rig.c.executed_frame==3 && rig.c.fault==SN_FAULT_NONE && rig.c.mode==SN_IDLE);
}
int main(int argc,char **argv) {
  if(argc==2 && !std::strcmp(argv[1],"--pio-plans")) {
    for(unsigned count : {1u,80u,166u,167u,200u})for(unsigned pulse : {1000u,1500u,2000u,2500u,10000u}) {
      uint32_t high,low;
      if(sn_step_plan(pulse,count,8400,&high,&low))
        std::printf("%u %u 8400 %u %u\n",count,pulse,high,low);
    }
    return 0;
  }
  assert(SN_MAX_STEP_RATE==200000 && SN_MAX_STEPS_TICK==200);
  assert(sn_step_rate_fits(166667,2500,5000));
  assert(sn_step_rate_fits(200000,2000,5000));
  assert(!sn_step_rate_fits(200000,2500,5000));
  sn_machine_config config,decoded;sn_config_defaults(&config);uint8_t bytes[128];
  for(auto &axis:config.axis)axis.max_velocity=200000;
  sn_config_encode(bytes,&config);assert(!sn_config_decode(bytes,sizeof(bytes),&decoded));
  config.pulse_ns=2000;sn_config_encode(bytes,&config);assert(sn_config_decode(bytes,sizeof(bytes),&decoded));
  config.axis[0].max_velocity=200001;sn_config_encode(bytes,&config);assert(!sn_config_decode(bytes,sizeof(bytes),&decoded));
  const int fast[3]={200,200,200},fractional[3]={167,166,167};
  rate_stream(200000,2000,fast,600);rate_stream(166667,2500,fractional,500);
  sn::WaypointAccumulator accumulator;int32_t steps[3];double delta[3]={200,-200,200};
  assert(accumulator.convert(delta,steps) && steps[0]==200 && steps[1]==-200);
  delta[0]=201;assert(!accumulator.convert(delta,steps));
  const double goal[3]={1,0,0},scale[3]={1000,1000,1000};const int64_t position[3]={0,0,0};uint8_t probe[32];
  assert(sn::probe_packet(probe,goal,scale,position,12000).empty());
  assert(sn_read_u32(probe+24)==200000);
  assert(!sn::probe_packet(probe,goal,scale,position,12001).empty());
  Rig rig;uint8_t status[SN_CONTROL_STATUS_SIZE];sn_controller_status(&rig.c,status,0);
  assert((status[102]&SN_STATUS_CAP_STEP_RATE) && sn_read_u32(status+103)==200000);
  std::puts("PASS: 200 kHz XYZ frames, 201-step rejection, 166.667 kHz fractional frames, pulse limits, G31 and rate capability.");
}
