#include "stdafx.h"
#include "mach3_abi.h"
#include "controller_receiver.h"
#include "../config/resource.h"
#include <cstdio>
#include <cstring>
#include <functional>

static bool wait_for(const std::function<bool()> &predicate) {
  for(int i=0;i<150;++i) {if(predicate())return true;std::this_thread::sleep_for(std::chrono::milliseconds(10));}
  return false;
}
static BOOL CALLBACK find_dialog(HWND window,LPARAM context) {
  if(!IsWindowVisible(window))return TRUE;
  char title[128]={};GetWindowTextA(window,title,sizeof(title));
  if(std::strcmp(title,"StepperNinja configuration"))return TRUE;
  *reinterpret_cast<HWND *>(context)=window;return FALSE;
}
static bool configure(void (__cdecl *config)(),Receiver &receiver,bool save,unsigned initial,unsigned changed) {
  const DWORD ui=GetCurrentThreadId();std::atomic<bool> passed{false};
  const unsigned claims=receiver.counts[SN_CLAIM],releases=receiver.counts[SN_RELEASE],arms=receiver.counts[SN_ARM];
  std::thread automation([&] {
    HWND dialog=nullptr;
    if(!wait_for([&]{EnumThreadWindows(ui,find_dialog,reinterpret_cast<LPARAM>(&dialog));return dialog!=nullptr;}))return;
    char state[32]={};
    bool ok=wait_for([&]{GetDlgItemTextA(dialog,IDC_SN_DI1_STATE,state,sizeof(state));return state[0]=='H'||state[0]=='L';});
    ok=ok && !IsWindowVisible(GetDlgItem(dialog,IDC_SN_MONITOR));
    const unsigned poll=receiver.counts[SN_POLL];
    // Stay in the dialog longer than the firmware watchdog. The original
    // session must remain alive, including when connection fields are edited.
    SetDlgItemTextA(dialog,IDC_SN_COM,"COM99");
    std::this_thread::sleep_for(std::chrono::milliseconds(800));
    ok=ok && receiver.owner!=0 && receiver.counts[SN_POLL]>poll+5;
    ok=ok && receiver.counts[SN_CLAIM]==claims && receiver.counts[SN_RELEASE]==releases && receiver.counts[SN_ARM]==arms;
    ok=ok && receiver.invert==initial;
    SendMessage(GetDlgItem(dialog,IDC_SN_DI1_INVERT),BM_SETCHECK,(changed&1)?BST_CHECKED:BST_UNCHECKED,0);
    SendMessage(dialog,WM_COMMAND,MAKEWPARAM(IDC_SN_DI1_INVERT,BN_CLICKED),reinterpret_cast<LPARAM>(GetDlgItem(dialog,IDC_SN_DI1_INVERT)));
    GetDlgItemTextA(dialog,IDC_SN_DI1_STATE,state,sizeof(state));
    const bool active=((receiver.raw.load()^changed)&1)!=0;
    ok=ok && std::strstr(state,active?"ON":"off")!=nullptr;
    passed=ok;
    PostMessage(dialog,WM_COMMAND,save?IDOK:IDCANCEL,0);
  });
  config();automation.join();
  return passed && receiver.owner!=0 && receiver.counts[SN_CLAIM]==claims && receiver.counts[SN_RELEASE]==releases && receiver.counts[SN_ARM]==arms;
}
static bool hardware_dialog(void (__cdecl *config)()) {
  const DWORD ui=GetCurrentThreadId();std::atomic<bool> passed{false};
  std::thread automation([&] {
    HWND dialog=nullptr;
    if(!wait_for([&]{EnumThreadWindows(ui,find_dialog,reinterpret_cast<LPARAM>(&dialog));return dialog!=nullptr;}))return;
    bool ok=!IsWindowVisible(GetDlgItem(dialog,IDC_SN_MONITOR));
    for(int cycle=0;cycle<15;++cycle) {
      for(int id=IDC_SN_DI1_STATE;id<=IDC_SN_DI7_STATE;++id) {
        char state[32]={};GetDlgItemTextA(dialog,id,state,sizeof(state));
        ok=ok && (state[0]=='H'||state[0]=='L');
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if(!ok) {
      for(int id : {IDC_SN_MONITOR_STATUS, IDC_SN_STATUS, IDC_SN_DI1_STATE}) {
        char text[512]={};GetDlgItemTextA(dialog,id,text,sizeof(text));
        std::fprintf(stderr,"Hardware dialog %d: %s\n",id,text);
      }
    }
    passed=ok;PostMessage(dialog,WM_COMMAND,IDCANCEL,0);
  });
  config();automation.join();return passed;
}
int main(int argc,char **argv) {
  if(argc==2 && !std::strcmp(argv[1], "--layout")) {
    std::printf("InSigs=%u InputInfo=%u EStop=%u Steps=%u Velocities=%u Acceleration=%u\n",
      unsigned(offsetof(TrajBuffer,InSigs)),unsigned(sizeof(InputInfo)),unsigned(offsetof(TrajBuffer,EStop)),
      unsigned(offsetof(TrajectoryControl,StepsPerAxis)),unsigned(offsetof(TrajectoryControl,Velocities)),unsigned(offsetof(TrajectoryControl,Acceleration)));
    return 0;
  }

  if((argc!=3 && argc!=4) || FAILED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)))return 2;
  WSADATA data;if(WSAStartup(MAKEWORD(2,2),&data))return 3;
  Receiver receiver;receiver.raw=127;
  if(!receiver.port)return 4;
  FILE *f=std::fopen(argv[2],"wb");if(!f)return 5;
  std::fprintf(f,"<profile><StepperNinjaMach3><IP>127.0.0.1</IP><Port>%u</Port><InputInvert>127</InputInvert></StepperNinjaMach3></profile>",receiver.port);
  std::fclose(f);
  if(argc==4) {
    f=std::fopen(argv[2],"wb");
    std::fprintf(f,"<profile><StepperNinjaMach3><USB>1</USB><COMPort>%s</COMPort><InputInvert>127</InputInvert></StepperNinjaMach3></profile>",argv[3]);
    std::fclose(f);
  }
  HMODULE module=LoadLibraryA(argv[1]);if(!module)return 6;
  auto profile=reinterpret_cast<char *(__cdecl *)(CString)>(GetProcAddress(module,"SetProName"));
  auto init=reinterpret_cast<bool (__cdecl *)(void *,void *,void *,void *)>(GetProcAddress(module,"InitControl"));
  auto post=reinterpret_cast<void (__cdecl *)()>(GetProcAddress(module,"PostInitControl"));
  auto update=reinterpret_cast<void (__cdecl *)()>(GetProcAddress(module,"Update"));
  auto config=reinterpret_cast<void (__cdecl *)()>(GetProcAddress(module,"Config"));
  auto cleanup=reinterpret_cast<void (__cdecl *)()>(GetProcAddress(module,"StopPlug"));
  if(!profile||!init||!post||!update||!config||!cleanup)return 7;
  // SDK objects are only shared memory here. No methods or constructors run.
  auto engine=static_cast<TrajBuffer *>(std::calloc(1,sizeof(TrajBuffer)));
  auto planner=static_cast<TrajectoryControl *>(std::calloc(1,sizeof(TrajectoryControl)));
  for(unsigned i=0;i<3;++i) {planner->StepsPerAxis[i]=100;planner->Velocities[i]=10000.0/60;planner->Acceleration[i]=250;}
  planner->PurgeMovements=true;
  engine->InSigs[EMERGENCY].Active=true;engine->InSigs[EMERGENCY].InPort=1;engine->InSigs[EMERGENCY].InPin=1;
  profile(CString(argv[2]));init(engine,nullptr,planner,nullptr);post();update();
  if(argc==4) {
    if(!engine->EStop || !hardware_dialog(config))return 20;
    update();
    if(!engine->EStop)return 21;
    cleanup();
    std::puts("PASS: real USB Pico inputs remain live for 1.5 seconds on the driver's shared connection; Cancel resumes polling, no ARM sent.");
    FreeLibrary(module);std::free(engine);std::free(planner);WSACleanup();CoUninitialize();return 0;
  }
  if(!engine->EStop || engine->InSigs[EMERGENCY].Activated || receiver.counts[SN_CLAIM]!=1 || receiver.invert!=127)return 8;
  if(!configure(config,receiver,false,127,126))return 9;
  update();if(engine->InSigs[EMERGENCY].Activated || receiver.invert!=127)return 10;
  if(!configure(config,receiver,true,127,126))return 11;
  update();if(!engine->InSigs[EMERGENCY].Activated || receiver.invert!=126)return 12;
  if(!configure(config,receiver,true,126,127))return 13;
  update();if(engine->InSigs[EMERGENCY].Activated || receiver.invert!=127)return 14;
  engine->EStop=false;update();if(engine->EStop || !receiver.armed)return 15;
  receiver.raw=126;std::this_thread::sleep_for(std::chrono::milliseconds(30));update();
  if(!engine->EStop || receiver.armed || receiver.fault!=SN_FAULT_ESTOP)return 16;
  update();receiver.raw=127;std::this_thread::sleep_for(std::chrono::milliseconds(30));update();
  if(engine->InSigs[EMERGENCY].Activated)return 17;
  engine->EStop=false;update();if(engine->EStop || !receiver.armed)return 18;
  cleanup();if(receiver.owner!=0)return 19;
  std::puts("PASS: config shares the driver's session; no extra CLAIM/RELEASE or ARM, watchdog stays alive.");
  std::puts("PASS: Cancel preserves polarity; Save updates both firmware and Mach3 E-stop signal.");
  std::puts("PASS: E-stop trips on real input, then clears only after input release and Reset.");
  FreeLibrary(module);std::free(engine);std::free(planner);WSACleanup();CoUninitialize();return 0;
}
