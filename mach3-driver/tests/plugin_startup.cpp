#include <afxwin.h>
#include <objbase.h>
#include <msxml.h>
#include <cstdio>
#include <cstring>

// Host intentionally has no CWinApp in the plugin's modern MFC runtime.
// Mach3 uses its own older MFC runtime, so plain LoadLibrary is insufficient.
int main(int argc, char **argv) {
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
  if (argc != 3) return 2;
  HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (FAILED(hr)) return 3;
  HMODULE module = LoadLibraryA(argv[1]);
  if (!module) return 4;
  typedef char *(__cdecl *ProfileInit)(CString);
  ProfileInit init = reinterpret_cast<ProfileInit>(GetProcAddress(module, "SetProName"));
  if (!init) return 5;
  std::puts("Calling SetProName from a host with no modern MFC application.");
  std::fflush(stdout);
  const char *name = init(CString(argv[2]));
  if (!name || !std::strstr(name, "StepperNinja")) return 6;
  std::printf("PASS: plugin profile startup returned: %s\n", name);
  // Mach3 persists the returned plugin identifier as an XML node name.
  // Invalid names make the legacy XML writer dereference a null entry.
  IXMLDOMDocument *document = nullptr;
  hr = CoCreateInstance(CLSID_DOMDocument, nullptr, CLSCTX_INPROC_SERVER,
                        IID_IXMLDOMDocument, reinterpret_cast<void **>(&document));
  if (FAILED(hr) || !document) return 8;
  IXMLDOMElement *entry = nullptr;
  hr = document->createElement(CComBSTR(name), &entry);
  if (FAILED(hr) || !entry) {
    std::fprintf(stderr, "Plugin identifier cannot be used as an XML node: %s\n", name);
    document->Release();
    return 9;
  }
  entry->Release();
  document->Release();
  std::puts("PASS: plugin activation identifier is a valid XML node name.");
  typedef bool (__cdecl *ControlInit)(void *, void *, void *, void *);
  ControlInit control = reinterpret_cast<ControlInit>(GetProcAddress(module, "InitControl"));
  if (!control || control(nullptr, nullptr, nullptr, nullptr)) return 7;
  std::puts("PASS: external controller does not request the printer-port engine.");
  FreeLibrary(module);
  CoUninitialize();
  return 0;
}