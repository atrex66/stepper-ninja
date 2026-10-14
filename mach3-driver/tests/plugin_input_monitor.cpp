#include <afxwin.h>
#include <afxdisp.h>
#include <afxsock.h>
#include <msxml.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <functional>
#include <thread>
#include "../../common/motion/controller.h"
#include "../config/resource.h"

static const int state_ids[7] = {IDC_SN_DI1_STATE,IDC_SN_DI2_STATE,IDC_SN_DI3_STATE,
    IDC_SN_DI4_STATE,IDC_SN_DI5_STATE,IDC_SN_DI6_STATE,IDC_SN_DI7_STATE};
#include "controller_receiver.h"
static BOOL CALLBACK find_dialog(HWND window,LPARAM context) {
  // Wait until OnInitDialog has completed before changing any fields.
  if (!IsWindowVisible(window)) return TRUE;
  char title[128]={};GetWindowTextA(window,title,sizeof(title));
  if(std::strcmp(title,"StepperNinja configuration"))return TRUE;
  *reinterpret_cast<HWND *>(context)=window;return FALSE;
}
static bool wait_for(const std::function<bool()> &predicate) {
  for(int i=0;i<100;++i) {if(predicate())return true;std::this_thread::sleep_for(std::chrono::milliseconds(20));}
  return false;
}
static bool states(HWND dialog,unsigned raw,unsigned invert,bool unknown=false) {
  for(unsigned i=0;i<7;++i) {
    char expected[32],actual[32]={};
    if(unknown)std::strcpy(expected,"-- / --");
    else std::sprintf(expected,"%s / %s",(raw&(1u<<i))?"H":"L",((raw^invert)&(1u<<i))?"ON":"off");
    GetDlgItemTextA(dialog,state_ids[i],actual,sizeof(actual));
    if(std::strcmp(expected,actual))return false;
  }
  return true;
}
static bool contains(HWND dialog,int id,const char *part) {
  char text[512]={};GetDlgItemTextA(dialog,id,text,sizeof(text));return std::strstr(text,part)!=nullptr;
}
static void click(HWND dialog,int id) {SendMessage(GetDlgItem(dialog,id),BM_CLICK,0,0);}
static bool run_dialog(void (__cdecl *config)(),Receiver *receiver,int scenario,const char *hardware=nullptr) {
  const DWORD ui_thread=GetCurrentThreadId();std::atomic<bool> passed(false);
  std::thread automation([&] {
    HWND dialog=nullptr;
    if(!wait_for([&]{EnumThreadWindows(ui_thread,find_dialog,reinterpret_cast<LPARAM>(&dialog));return dialog!=nullptr;}))return;
    bool ok=states(dialog,0,0,true);
    if(hardware) {SendMessage(GetDlgItem(dialog,IDC_SN_USB),BM_SETCHECK,BST_CHECKED,0);SetDlgItemTextA(dialog,IDC_SN_COM,hardware);}
    else {SetDlgItemTextA(dialog,IDC_SN_IP,"127.0.0.1");SetDlgItemInt(dialog,IDC_SN_PORT,receiver->port,FALSE);}
    SetDlgItemInt(dialog,IDC_SN_TIMEOUT,200,FALSE);
    click(dialog,IDC_SN_MONITOR);
    if(scenario==1) {
      ok=ok && wait_for([&]{return contains(dialog,IDC_SN_MONITOR_STATUS,"updated firmware");}) && states(dialog,0,0,true);
    } else if(hardware) {
      ok=ok && wait_for([&]{return contains(dialog,IDC_SN_MONITOR_STATUS,"Live inputs");});
      for(int id:state_ids) {
        char text[32]={};GetDlgItemTextA(dialog,id,text,sizeof(text));
        ok=ok && (text[0]=='H'||text[0]=='L');
        std::printf("%s ",text);
      }
      std::puts("");
    } else {
      ok=ok && wait_for([&]{return states(dialog,42,127);});
      if(scenario==2) {
        receiver->drop=true;
        ok=ok && wait_for([&]{return contains(dialog,IDC_SN_MONITOR_STATUS,"timeout");}) && states(dialog,0,0,true);
        ok=ok && wait_for([&]{return receiver->owner.load()==0;});
      } else {
        click(dialog,IDC_SN_DI1_INVERT);
        ok=ok && states(dialog,42,126);
        receiver->raw=85;
        ok=ok && wait_for([&]{return states(dialog,85,126);});
        click(dialog,IDC_SN_MONITOR);
        ok=ok && states(dialog,0,0,true) && receiver->owner.load()==0;
        click(dialog,IDC_SN_MONITOR);
        ok=ok && wait_for([&]{return states(dialog,85,126);});
      }
    }
    if(!ok) {
      char status[512]={};GetDlgItemTextA(dialog,IDC_SN_MONITOR_STATUS,status,sizeof(status));
      std::fprintf(stderr,"Scenario %d status: %s; owner=%u; raw=%u\n",scenario,status,
                   receiver?receiver->owner.load():0,receiver?receiver->raw.load():0);
      for(int id:state_ids) {char state[40]={};GetDlgItemTextA(dialog,id,state,sizeof(state));std::fprintf(stderr,"%s ",state);}
      std::fprintf(stderr,"\n");
    }
    passed=ok;PostMessage(dialog,WM_COMMAND,IDCANCEL,0);
  });
  config();automation.join();
  if(receiver && (!receiver->readonly() || receiver->counts[SN_CLAIM]==0 || receiver->counts[SN_POLL]==0))return false;
  return passed;
}
int main(int argc,char **argv) {
  SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
  if(argc<3 || FAILED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)))return 2;
  WSADATA winsock;if(WSAStartup(MAKEWORD(2,2),&winsock))return 3;
  FILE *file=std::fopen(argv[2],"wb");if(!file)return 4;
  std::fputs("<profile><StepperNinjaMach3><InputInvert>127</InputInvert></StepperNinjaMach3></profile>",file);std::fclose(file);
  HMODULE module=LoadLibraryA(argv[1]);if(!module)return 5;
  auto init=reinterpret_cast<char *(__cdecl *)(CString)>(GetProcAddress(module,"SetProName"));
  auto config=reinterpret_cast<void (__cdecl *)()>(GetProcAddress(module,"Config"));
  if(!init||!config)return 6;init(CString(argv[2]));
  if(argc==4) {
    if(!run_dialog(config,nullptr,0,argv[3]))return 7;
    std::puts("PASS: real device reports live raw inputs without arming; closing releases connection.");
  } else {
    for(int scenario=0;scenario<3;++scenario) {
      Receiver receiver;if(!receiver.port)return 8;receiver.legacy=(scenario==1);
      if(!run_dialog(config,&receiver,scenario)){std::fprintf(stderr,"Input monitor scenario %d failed.\n",scenario);return 9;}
      if(scenario!=2 && receiver.owner.load()!=0)return 10;
    }
    std::puts("PASS: all 7 raw/logical inputs refresh; unsaved inversion previews immediately.");
    std::puts("PASS: Stop/Cancel release ownership; legacy firmware and timeouts clear stale states.");
    std::puts("PASS: monitor only sends CLAIM/POLL/RELEASE; never CONFIG, ARM or motion commands.");
  }
  FreeLibrary(module);WSACleanup();CoUninitialize();return 0;
}