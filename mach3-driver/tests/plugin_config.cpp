#include <afxwin.h>
#include <afxdisp.h>
#include <objbase.h>
#include <msxml.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>
#include "../config/resource.h"

static const int controls[7] = {IDC_SN_DI1_INVERT, IDC_SN_DI2_INVERT,
    IDC_SN_DI3_INVERT, IDC_SN_DI4_INVERT, IDC_SN_DI5_INVERT,
    IDC_SN_DI6_INVERT, IDC_SN_DI7_INVERT};
static BOOL CALLBACK find_dialog(HWND window, LPARAM context) {
  // Wait until OnInitDialog has completed before changing any fields.
  if (!IsWindowVisible(window)) return TRUE;
  char title[128] = {};
  GetWindowTextA(window, title, sizeof(title));
  if (std::strcmp(title, "StepperNinja configuration") != 0) return TRUE;
  *reinterpret_cast<HWND *>(context) = window;
  return FALSE;
}
static bool configure(void (__cdecl *config)(), unsigned initial,
                      unsigned changed, bool save) {
  const DWORD ui_thread = GetCurrentThreadId();
  std::atomic<int> result(0);
  std::thread driver([&] {
    HWND dialog = nullptr;
    for (int attempt = 0; attempt < 100 && !dialog; ++attempt) {
      EnumThreadWindows(ui_thread, find_dialog, reinterpret_cast<LPARAM>(&dialog));
      if (!dialog) std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    if (!dialog) { result = 1; return; }
    RECT client = {};
    GetClientRect(dialog, &client);
    for (unsigned i = 0; i < 7; ++i) {
      HWND checkbox = GetDlgItem(dialog, controls[i]);
      if (!checkbox || SendMessage(checkbox, BM_GETCHECK, 0, 0) !=
          ((initial & (1u << i)) ? BST_CHECKED : BST_UNCHECKED)) {
        result = 2; break;
      }
      RECT bounds = {};
      GetWindowRect(checkbox, &bounds);
      MapWindowPoints(nullptr, dialog, reinterpret_cast<POINT *>(&bounds), 2);
      if (bounds.left < 0 || bounds.top < 0 || bounds.right > client.right ||
          bounds.bottom > client.bottom) { result = 3; break; }
      SendMessage(checkbox, BM_SETCHECK,
                  (changed & (1u << i)) ? BST_CHECKED : BST_UNCHECKED, 0);
    }
    PostMessage(dialog, WM_COMMAND, save && !result ? IDOK : IDCANCEL, 0);
  });
  config();
  driver.join();
  if (result) std::fprintf(stderr, "Dialog test failed: %d\n", result.load());
  return result == 0;
}
static int read_value(const char *file, const char *xpath) {
  IXMLDOMDocument *document = nullptr;
  HRESULT hr = CoCreateInstance(CLSID_DOMDocument, nullptr, CLSCTX_INPROC_SERVER,
      IID_IXMLDOMDocument, reinterpret_cast<void **>(&document));
  if (FAILED(hr) || !document) return -999;
  VARIANT_BOOL loaded = VARIANT_FALSE;
  hr = document->load(COleVariant(file), &loaded);
  IXMLDOMNode *node = nullptr;
  if (SUCCEEDED(hr) && loaded) document->selectSingleNode(CComBSTR(xpath), &node);
  int value = -999;
  if (node) {
    BSTR text = nullptr;
    if (SUCCEEDED(node->get_text(&text)) && text) value = _wtoi(text);
    SysFreeString(text);
    node->Release();
  }
  document->Release();
  return value;
}
static bool check_saved(const char *file, unsigned mask) {
  if (read_value(file, "/profile/StepperNinjaMach3/InputInvert") != int(mask))
    return false;
  // X limit/home aliases share DI2; other DIs and an unrelated port-2 input.
  const int indices[] = {25,0,2,3,6,22,18,19};
  const int pins[] = {1,2,2,3,4,5,6,7};
  for (unsigned i = 0; i < sizeof(indices)/sizeof(indices[0]); ++i) {
    char xpath[128];
    std::sprintf(xpath, "/profile/Preferences/Input%dNeg", indices[i]);
    if (read_value(file, xpath) != int((mask >> (pins[i]-1)) & 1)) return false;
  }
  return read_value(file, "/profile/Preferences/Input20Neg") == 0;
}
int main(int argc, char **argv) {
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
  if (argc != 3 || FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 2;
  FILE *xml = std::fopen(argv[2], "wb");
  if (!xml) return 3;
  std::fputs("<profile><StepperNinjaMach3><InputInvert>127</InputInvert>"
             "</StepperNinjaMach3><Preferences>", xml);
  const int indices[] = {25,0,2,3,6,22,18,19};
  const int pins[] = {1,2,2,3,4,5,6,7};
  for (unsigned i = 0; i < sizeof(indices)/sizeof(indices[0]); ++i)
    std::fprintf(xml,"<Input%dPort>1</Input%dPort><Input%dPin>%d</Input%dPin>"
                    "<Input%dNeg>0</Input%dNeg>",indices[i],indices[i],indices[i],
                    pins[i],indices[i],indices[i],indices[i]);
  std::fputs("<Input20Port>2</Input20Port><Input20Pin>1</Input20Pin>"
             "<Input20Neg>0</Input20Neg></Preferences></profile>", xml);
  std::fclose(xml);
  HMODULE module = LoadLibraryA(argv[1]);
  if (!module) return 4;
  typedef char *(__cdecl *ProfileInit)(CString);
  ProfileInit init = reinterpret_cast<ProfileInit>(GetProcAddress(module,"SetProName"));
  auto config = reinterpret_cast<void (__cdecl *)()>(GetProcAddress(module,"Config"));
  if (!init || !config) return 5;
  init(CString(argv[2]));
  if (!configure(config,127,42,false) ||
      read_value(argv[2],"/profile/StepperNinjaMach3/InputInvert") != 127 ||
      read_value(argv[2],"/profile/Preferences/Input25Neg") != 0) return 6;
  unsigned previous = 127;
  for (unsigned mask : {42u,85u,0u,127u}) {
    if (!configure(config,previous,mask,true) || !check_saved(argv[2],mask)) return 7;
    previous = mask;
  }
  if (!configure(config,127,0,false) || !check_saved(argv[2],127)) return 8;
  std::puts("PASS: legacy mask loads all 7 checkboxes; Save persists alternating/all/none masks.");
  std::puts("PASS: Mach3 Active Low aliases synchronized; unrelated port preserved; Cancel preserves settings.");
  FreeLibrary(module);
  CoUninitialize();
  return 0;
}