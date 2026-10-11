#include "stdafx.h" // MFC must precede vendor SDK headers.

#include "../config/resource.h"
#include "Engine.h"
#include "Mach4View.h"
#include "TrajectoryControl.h"
#include "XMLProfile.h"
#include "controller_client.h"
#include "rs274ngc.h"
#include "settings.h"
#include "udp_probe.h"
#include "streamer.h"
#include "probe_motion.h"
#include <cmath>
#include <cstdlib>
#include <limits>
extern TrajectoryControl *MainPlanner;
extern CMach4View *MachView;
extern TrajBuffer *Engine;
extern setup *_setup;
static sn::Settings settings;
static sn::ControllerClient client;
static sn::Streamer streamer;
static bool probe_waiting = false, probe_running = false, stop_waiting = false;
static bool probe_cancel_waiting = false;
static uint32_t probe_token = 0;
static uint8_t probe_payload[32];
static bool selected = false, paused = false, busy = false;
static bool fault_seen = false;
static UINT_PTR timer_id = 0;
static unsigned homes_waiting = 0;
static uint32_t dwell_waiting = 0;
static const char *section = "StepperNinjaMach3";
struct UIntField {
  const char *key;
  unsigned sn::Settings::*member;
  int control;
};
static const UIntField fields[] = {
    {"Port", &sn::Settings::port, IDC_SN_PORT},
    {"ReplyTimeoutMs", &sn::Settings::timeout_ms, IDC_SN_TIMEOUT},
    {"PWMHz", &sn::Settings::pwm_hz, IDC_SN_PWM},
    {"WatchdogMs", &sn::Settings::watchdog_ms, IDC_SN_WATCHDOG},
    {"PulseNs", &sn::Settings::pulse_ns, IDC_SN_PULSE},
    {"DirSetupNs", &sn::Settings::dir_setup_ns, IDC_SN_SETUP},
    {"InputInvert", &sn::Settings::input_invert, IDC_SN_INPUT_INVERT},
    {"OutputInvert", &sn::Settings::output_invert, IDC_SN_OUTPUT_INVERT},
    {"DirectionInvert", &sn::Settings::direction_invert, IDC_SN_DIR_INVERT},
    {"HomePositive", &sn::Settings::home_positive, IDC_SN_HOME_POSITIVE},
    {"HomeSlowPercent", &sn::Settings::home_slow_percent, IDC_SN_HOME_SLOW}};
static double parse_double(const CString &text) {
  char *end = nullptr;
  double v = std::strtod((LPCSTR)text, &end);
  if (end == (LPCSTR)text || *end)
    return std::numeric_limits<double>::quiet_NaN();
  return v;
}
static CString double_text(double value) {
  CString text;
  text.Format("%.17g", value);
  return text;
}
static sn::Settings load(CXMLProfile *profile) {
  sn::Settings value;
  value.ip = (LPCSTR)profile->GetProfileString(section, "IP", value.ip.c_str());
  for (const auto &field : fields)
    value.*(field.member) =
        profile->GetProfileInt(section, field.key, value.*(field.member));
  value.home_backoff = parse_double(profile->GetProfileString(
      section, "HomeBackoff", double_text(value.home_backoff)));
  value.home_travel = parse_double(profile->GetProfileString(
      section, "HomeTravel", double_text(value.home_travel)));
  return value;
}
class ConfigDialog : public CDialog {
public:
  sn::Settings value;
  explicit ConfigDialog(const sn::Settings &initial)
      : CDialog(IDD_SN_CONFIG), value(initial) {}
  BOOL OnInitDialog() override {
    CDialog::OnInitDialog();
    SetDlgItemText(IDC_SN_IP, value.ip.c_str());
    for (const auto &field : fields)
      SetDlgItemInt(field.control, value.*(field.member));
    SetDlgItemText(IDC_SN_HOME_BACKOFF, double_text(value.home_backoff));
    SetDlgItemText(IDC_SN_HOME_TRAVEL, double_text(value.home_travel));
    return TRUE;
  }
  bool read() {
    CString ip;
    GetDlgItemText(IDC_SN_IP, ip);
    value.ip = (LPCSTR)ip;
    for (const auto &field : fields)
      value.*(field.member) = GetDlgItemInt(field.control);
    CString number;
    GetDlgItemText(IDC_SN_HOME_BACKOFF, number);
    value.home_backoff = parse_double(number);
    GetDlgItemText(IDC_SN_HOME_TRAVEL, number);
    value.home_travel = parse_double(number);
    std::string error = sn::validate_settings(value);
    if (!error.empty()) {
      SetDlgItemText(IDC_SN_STATUS, error.c_str());
      return false;
    }
    return true;
  }
  void OnOK() override {
    if (read())
      CDialog::OnOK();
  }
  afx_msg void test() {
    if (!read())
      return;
    GetDlgItem(IDC_SN_TEST)->EnableWindow(FALSE);
    std::string error = sn::probe_controller(value);
    SetDlgItemText(IDC_SN_STATUS,
                   error.empty() ? "Matching StepperNinja profile responded."
                                 : error.c_str());
    GetDlgItem(IDC_SN_TEST)->EnableWindow(TRUE);
  }
  DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(ConfigDialog, CDialog)
ON_BN_CLICKED(IDC_SN_TEST, &ConfigDialog::test)
END_MESSAGE_MAP()
static void clear_local_motion() {
  streamer.reset();
  probe_waiting = probe_running = false;
  probe_token = 0;
  probe_cancel_waiting = false;
  homes_waiting = 0;
  dwell_waiting = 0;
}
static void fail(const std::string &message) {
  if (client.connected())
    client.command(SN_ABORT);
  if (Engine) {
    Engine->EStop = true;
    Engine->Homing = Engine->Jogging = false;
    Engine->DwellTime = 0;
    Engine->Axis[0].Probing = false;
  }
  if (MainPlanner)
    MainPlanner->SoftWait &= ~(uint64_t(1) << EXHOME);
  clear_local_motion();
  OutputDebugStringA(("StepperNinja: " + message + "\n").c_str());
}
static bool command(unsigned type, const uint8_t *p = nullptr,
                    unsigned size = 0) {
  std::string error = client.command(static_cast<uint16_t>(type), p,
                                     static_cast<uint16_t>(size));
  if (!error.empty()) {
    fail(error);
    return false;
  }
  return true;
}
static bool machine_config(sn_machine_config &config) {
  sn_config_defaults(&config);
  config.watchdog_ms = settings.watchdog_ms;
  config.pwm_hz = settings.pwm_hz;
  config.pulse_ns = settings.pulse_ns;
  config.dir_setup_ns = settings.dir_setup_ns;
  config.input_invert = static_cast<uint8_t>(settings.input_invert);
  config.output_invert = static_cast<uint8_t>(settings.output_invert);
  config.dir_invert = static_cast<uint8_t>(settings.direction_invert);
  for (unsigned i = 0; i < 3; ++i) {
    double scale = MainPlanner->StepsPerAxis[i],
           velocity = MainPlanner->Velocities[i] * scale;
    double acceleration = MainPlanner->Acceleration[i] * scale;
    double backoff = settings.home_backoff * scale,
           travel = settings.home_travel * scale;
    if (!std::isfinite(scale) || !std::isfinite(velocity) ||
        !std::isfinite(acceleration) || scale <= 0 || velocity < 1 ||
        velocity > 80000 || acceleration < 1 || acceleration > 10000000 ||
        !std::isfinite(backoff) || !std::isfinite(travel) || backoff < 1 ||
        travel > 100000000) {
      fail("Check X/Y/Z Motor Tuning and homing distances.");
      return false;
    }
    sn_axis_config &a = config.axis[i];
    a.max_velocity = static_cast<uint32_t>(velocity);
    a.acceleration = static_cast<uint32_t>(acceleration);
    a.backoff = static_cast<uint32_t>(backoff);
    a.home_travel = static_cast<uint32_t>(travel);
    a.home_offset = Engine->RefCounts[i];
    a.home_direction = (settings.home_positive & (1u << i)) ? 1 : -1;
    unsigned percent =
        MainPlanner->RefSpeed[i] > 0 ? MainPlanner->RefSpeed[i] : 10;
    if (percent > 100) {
      fail("Invalid Mach3 home speed percentage.");
      return false;
    }
    a.home_fast =
        static_cast<uint32_t>((uint64_t(a.max_velocity) * percent) / 100);
    if (!a.home_fast)
      a.home_fast = 1;
    a.home_slow = static_cast<uint32_t>(
        (uint64_t(a.home_fast) * settings.home_slow_percent) / 100);
    if (!a.home_slow)
      a.home_slow = 1;
  }
  uint8_t bytes[128];
  sn_config_encode(bytes, &config);
  sn_machine_config validated;
  if (!sn_config_decode(bytes, sizeof(bytes), &validated)) {
    fail("Motor tuning exceeds pulse timing or homing limits.");
    return false;
  }
  return true;
}
static bool connect_controller() {
  std::string error = client.open(settings);
  if (!error.empty()) {
    fail(error);
    return false;
  }
  sn_machine_config config;
  if (!machine_config(config)) {
    client.close();
    return false;
  }
  uint8_t bytes[128];
  sn_config_encode(bytes, &config);
  if (!command(SN_CONFIG, bytes, sizeof(bytes)))
    return false;
  uint8_t positions[24];
  for (unsigned i = 0; i < 3; ++i)
    sn_write_i64(positions + i * 8, Engine->Axis[i].Index);
  if (!command(SN_SET_POSITION, positions, sizeof(positions)))
    return false;
  clear_local_motion();
  return true;
}
static void report_to_mach() {
  const sn::Report &r = client.report();
  for (unsigned i = 0; i < 3; ++i) {
    if (r.position[i] < INT32_MIN || r.position[i] > INT32_MAX) {
      fail("Position exceeds Mach3 SDK integer range.");
      return;
    }
    Engine->Axis[i].Index = static_cast<int>(r.position[i]);
    Engine->Referenced[i] = (r.homed & (1u << i)) != 0;
  }
  Engine->DisplayLine = static_cast<int>(r.line);
  MainPlanner->ELine = static_cast<int>(r.line);
  uint8_t raw = r.inputs ^ static_cast<uint8_t>(settings.input_invert);
  for (int i = 0; i < nSigs; ++i) {
    if (Engine->InSigs[i].Active && Engine->InSigs[i].InPort == 1 &&
        Engine->InSigs[i].InPin >= 1 && Engine->InSigs[i].InPin <= 7) {
      bool level = ((raw >> (Engine->InSigs[i].InPin - 1)) & 1) != 0;
      Engine->InSigs[i].Activated = level != Engine->InSigs[i].Negated;
    }
  }
  bool was_still = MainPlanner->ExternalStill;
  MainPlanner->ExternalStill = r.mode == SN_IDLE && !r.pending && !r.fill;
  Engine->Jogging = r.mode == SN_JOGGING || r.mode == SN_POSITIONING;
  Engine->Homing = r.mode == SN_HOMING || homes_waiting != 0;
  if (!Engine->Homing)
    MainPlanner->SoftWait &= ~(uint64_t(1) << EXHOME);
  if (MainPlanner->ExternalStill && !was_still && !probe_running) {
    Engine->Sync = true;
    streamer.reset();
    if (!dwell_waiting)
      Engine->DwellTime = 0;
  }
}
static bool send_outputs() {
  uint8_t outputs = 0;
  if (MainPlanner->Spindle.SpindleOn)
    outputs |= 2;
  if (MainPlanner->Spindle.Direction)
    outputs |= 4;
  if (MainPlanner->Spindle.MistOn)
    outputs |= 8;
  if (MainPlanner->Spindle.FloodOn)
    outputs |= 16;
  for (int i = 0; i < nSigsOut; ++i)
    if (Engine->OutSigs[i].active && Engine->OutSigs[i].OutPort == 1 &&
        Engine->OutSigs[i].OutPin == 6 && Engine->OutSigs[i].Activated)
      outputs |= 32;
  double ratio =
      MainPlanner->Spindle.SpindleOn ? MainPlanner->Spindle.ratio : 0;
  if (!std::isfinite(ratio)) {
    fail("Invalid spindle PWM ratio.");
    return false;
  }
  if (ratio < 0)
    ratio = 0;
  if (ratio > 1)
    ratio = 1;
  unsigned duty = static_cast<unsigned>(std::round(ratio * 65535));
  uint8_t payload[4] = {outputs, static_cast<uint8_t>(duty),
                        static_cast<uint8_t>(duty >> 8), 0};
  return command(SN_IO, payload, sizeof(payload));
}
class MachTrajectory : public sn::TrajectorySource {
public:
  unsigned available() const override {
    return (Engine->TrajHead - Engine->TrajIndex) & 0xfff;
  }
  sn::Waypoint peek(unsigned offset) const override {
    unsigned index = (Engine->TrajIndex + offset) & 0xfff;
    const GMoves &m = MainPlanner->Movements[index];
    sn::Waypoint w;
    w.delta[0] = m.ex;
    w.delta[1] = m.ey;
    w.delta[2] = m.ez;
    w.delta[3] = m.ea;
    w.delta[4] = m.eb;
    w.delta[5] = m.ec;
    w.type = m.type;
    w.stop = m.Stop;
    w.line = Engine->Trajectories[index].ID;
    return w;
  }
  void consume(unsigned count) override {
    Engine->TrajIndex = (Engine->TrajIndex + count) & 0xfff;
  }
  bool ending() const override { return MainPlanner->BufferEnding; }
};
static bool send_trajectory() {
  MachTrajectory source;
  std::string error = streamer.update(client, source);
  if (!error.empty()) {
    fail(error);
    return false;
  }
  return true;
}
static bool finish_probe() {
  const sn::Report &r = client.report();
  if (!probe_running)
    return true;
  if (r.probe_id != probe_token) {
    fail("Probe report token mismatch.");
    return false;
  }
  if (r.mode != SN_IDLE || r.pending)
    return false;
  if (r.probe_state != SN_PROBE_HIT) {
    fail("Probe move ended without contact.");
    return false;
  }
  // SDK ncPod releases Axis[0].Probing only after its captured result is
  // available. Publish both SDK probe coordinates and parameters before
  // releasing that wait.
  double result[3];
  std::string error = sn::probe_work_position(result, r.probe_position,
                                              MainPlanner->StepsPerAxis,
                                              _setup->CurrentOrigins);
  if (!error.empty()) {
    fail(error);
    return false;
  }
  double *sdk_positions[3] = {&MainPlanner->ProbeX, &MainPlanner->ProbeY,
                              &MainPlanner->ProbeZ};
  for (unsigned i = 0; i < 3; ++i) {
    if (r.probe_position[i] < INT32_MIN || r.probe_position[i] > INT32_MAX) {
      fail("Probe latch exceeds SDK integer range.");
      return false;
    }
    Engine->Axis[i].DepthCount = static_cast<int>(r.probe_position[i]);
    *sdk_positions[i] = result[i];
    _setup->parameters[2000 + i] = result[i];
  }
  // Clear only the old G31 path; next commands remain blocked by Probing until
  // now.
  MainPlanner->PurgeMovements = true;
  Engine->TrajIndex = Engine->TrajHead;
  streamer.reset();
  Engine->Sync = true;
  Engine->Axis[0].Probing = false;
  probe_running = false;
  return true;
}
static void update_controller() {
  if (!selected || paused || !Engine || !MainPlanner || busy)
    return;
  busy = true;
  struct Guard {
    ~Guard() { busy = false; }
  } guard;
  if (!client.connected()) {
    Engine->EStop = true;
    if (!connect_controller())
      return;
  }
  if (Engine->EStop || Engine->OffLine) {
    if (client.report().armed && !command(SN_DISARM))
      return;
    if (!command(SN_POLL))
      return;
    clear_local_motion();
    stop_waiting = false;
    Engine->Axis[0].Probing = false;
    fault_seen = client.report().fault != 0;
    report_to_mach();
    return;
  }
  // Observe a newly latched hardware fault before ARM can clear it. A Reset
  // cycle acknowledges it; reconnect always starts with EStop asserted.
  if (!command(SN_POLL))
    return;
  if (client.report().fault && !fault_seen) {
    fault_seen = true;
    fail("Firmware fault " + std::to_string(client.report().fault));
    return;
  }
  if (!client.report().armed) {
    sn_machine_config config;
    if (!machine_config(config))
      return;
    uint8_t bytes[128];
    sn_config_encode(bytes, &config);
    if (!command(SN_CONFIG, bytes, sizeof(bytes)) || !command(SN_ARM))
      return;
    fault_seen = false;
  }
  report_to_mach();
  if (Engine->EStop)
    return;
  if (stop_waiting) {
    stop_waiting = false;
    if (!command(SN_ABORT))
      return;
    clear_local_motion();
    Engine->Axis[0].Probing = false;
    Engine->Homing = Engine->Jogging = false;
    MainPlanner->SoftWait &= ~(uint64_t(1) << EXHOME);
    MainPlanner->PurgeMovements = true;
    Engine->TrajIndex = Engine->TrajHead;
    Engine->Sync = true;
    return;
  }
  // FeedHolding is Mach3's native planner state. Continue pumping its
  // deceleration records; HOLD/ABORT/Purge would destroy the planner's
  // resumable endpoint.
  if (probe_cancel_waiting && probe_waiting) {
    fail("G31 cancelled before motion.");
    return;
  }
  if (!send_outputs())
    return;
  if (probe_cancel_waiting && probe_running) {
    if (!command(SN_PROBE_CANCEL))
      return;
    probe_cancel_waiting = false;
  }
  if (probe_running) {
    if (!finish_probe())
      return;
    return; // Let Mach3 observe completion before consuming a following
            // command.
  }
  if (client.report().mode == SN_IDLE && !client.report().pending &&
      !client.report().fill) {
    if (probe_waiting) {
      if (!command(SN_PROBE, probe_payload, sizeof(probe_payload)))
        return;
      probe_waiting = false;
      probe_running = true;
      probe_token = client.report().probe_id;
      MainPlanner->ExternalStill = false;
      return;
    }
    if (homes_waiting) {
      unsigned axis = 0;
      while (!(homes_waiting & (1u << axis)))
        ++axis;
      uint8_t payload[4] = {static_cast<uint8_t>(axis), 0, 0, 0};
      if (!command(SN_HOME, payload, sizeof(payload)))
        return;
      homes_waiting &= ~(1u << axis);
      Engine->Homing = true;
      return;
    }
    if (dwell_waiting) {
      uint8_t payload[4];
      sn_write_u32(payload, dwell_waiting);
      if (!command(SN_DWELL, payload, sizeof(payload)))
        return;
      dwell_waiting = 0;
      return;
    }
  }
  if (!MainPlanner->PurgeMovements)
    send_trajectory();
}
static VOID CALLBACK update_timer(HWND, UINT, UINT_PTR, DWORD) {
  AFX_MANAGE_STATE(AfxGetStaticModuleState());
  update_controller();
}
CString myProfileInit(CString, CXMLProfile *profile) {
  settings = load(profile);
  return " StepperNinja UDP 0.2 (3-axis development)";
}
void myInitControl() {
  if (MachView)
    MachView->m_PrinterOn = false;
  if (MainPlanner) {
    MainPlanner->DoExternalProbe = true;
    MainPlanner->m_ExternalFeedHold =
        false; // Mach3 plans the resumable braking path.
    MainPlanner->ExternalType = EX_VMS;
    MainPlanner->ExTime = .001;
    MainPlanner->ExBufferHi = 100;
    for (unsigned i = 0; i < 7; ++i)
      MainPlanner->ExternalPulseRates[i] = i < 3 ? 80000 : 0;
  }
}
void myPostInitControl() {
  myInitControl(); // Reinstate transport capabilities after profile loading.
  selected = true;
  if (Engine)
    Engine->EStop = true;
  timer_id = SetTimer(nullptr, 0, 25, update_timer);
  if (!timer_id)
    fail("Cannot start Mach3 update timer.");
}
void myConfig(CXMLProfile *profile) {
  if (selected && Engine && !Engine->EStop) {
    AfxMessageBox("Press Reset before configuration.");
    return;
  }
  paused = true;
  if (client.connected()) {
    client.command(SN_DISARM);
    client.command(SN_RELEASE);
    client.close();
  }
  ConfigDialog dialog(load(profile));
  if (dialog.DoModal() == IDOK) {
    settings = dialog.value;
    profile->WriteProfileString(section, "IP", settings.ip.c_str());
    for (const auto &field : fields)
      profile->WriteProfileInt(section, field.key, settings.*(field.member));
    profile->WriteProfileString(section, "HomeBackoff",
                                double_text(settings.home_backoff));
    profile->WriteProfileString(section, "HomeTravel",
                                double_text(settings.home_travel));
  }
  paused = false;
}
void myUpdate() { update_controller(); }
void myHighSpeedUpdate() { update_controller(); }
void myCleanUp() {
  selected = false;
  if (timer_id)
    KillTimer(nullptr, timer_id);
  timer_id = 0;
  if (client.connected()) {
    client.command(SN_DISARM);
    client.command(SN_RELEASE);
  }
  client.close();
}
void MyJogOn(short axis, short direction, double speed) {
  if (axis < 0 || axis >= 3 || Engine->EStop || !client.connected())
    return;
  if (Engine->TrajHead != Engine->TrajIndex ||
      (!MainPlanner->ExternalStill && client.report().mode != SN_JOGGING)) {
    fail("Jog requested while another move is active.");
    return;
  }
  double velocity = MainPlanner->Velocities[axis];
  if (speed != 0)
    velocity = speed;
  else if (!(*MachView->m_ShiftState))
    velocity *= _setup->JogPercent / 100;
  velocity *= MainPlanner->StepsPerAxis[axis];
  if (!std::isfinite(velocity) || velocity < 1 || velocity > 80000) {
    fail("Invalid jog velocity.");
    return;
  }
  uint8_t payload[8] = {static_cast<uint8_t>(axis),
                        static_cast<uint8_t>(direction ? 1 : 255),
                        0,
                        0,
                        0,
                        0,
                        0,
                        0};
  sn_write_u32(payload + 4, static_cast<uint32_t>(velocity));
  if (command(SN_JOG, payload, sizeof(payload)))
    Engine->Jogging = true;
}
void MyJogOff(short axis) {
  if (axis < 0 || axis >= 3 || !client.connected() ||
      client.report().mode != SN_JOGGING)
    return;
  uint8_t payload[4] = {static_cast<uint8_t>(axis), 0, 0, 0};
  command(SN_JOG_STOP, payload, sizeof(payload));
}
void myHome(short axis) {
  if (axis < 0 || axis >= 3 || Engine->EStop)
    return;
  if (Engine->TrajHead != Engine->TrajIndex || client.report().fill ||
      (client.report().mode != SN_IDLE && client.report().mode != SN_HOMING)) {
    fail("Homing requested while another move is active.");
    return;
  }
  homes_waiting |= 1u << axis;
  Engine->Homing = true;
  MainPlanner->SoftWait |= uint64_t(1) << EXHOME;
}
void myProbe() {
  if (!selected || !Engine || !MainPlanner || Engine->EStop ||
      !client.connected())
    return;
  if (probe_waiting || probe_running || homes_waiting ||
      !MainPlanner->ExternalStill || client.report().fill ||
      client.report().pending) {
    fail("Probe requires idle XYZ axes.");
    return;
  }
  if (!Engine->InSigs[DIGITIZE].Active ||
      Engine->InSigs[DIGITIZE].InPort != 1 ||
      Engine->InSigs[DIGITIZE].InPin != 5 ||
      Engine->InSigs[DIGITIZE].Negated != ((settings.input_invert & 16) != 0)) {
    fail("Set Probe to port 1/pin 5 (GP26), with matching InputInvert "
         "polarity.");
    return;
  }
  if (MainPlanner->aProbeA || MainPlanner->aProbeB || MainPlanner->aProbeC) {
    fail("Rotary probing is unsupported.");
    return;
  }
  if (_setup->feed_mode != UNITS_PER_MINUTE) {
    fail("G31 requires G94 feed in units/minute.");
    return;
  }
  double goal[3] = {MainPlanner->aProbeX, MainPlanner->aProbeY,
                    MainPlanner->aProbeZ};
  std::string error =
      sn::probe_packet(probe_payload, goal, MainPlanner->StepsPerAxis,
                       client.report().position, _setup->feed_rate);
  if (!error.empty()) {
    fail(error);
    return;
  }
  probe_waiting = true;
  Engine->Axis[0].Probing = true;
}
extern "C" __declspec(dllexport) void Notify(int message) {
  AFX_MANAGE_STATE(AfxGetStaticModuleState());
  if (message == -1 || message == EX_ESTOP)
    stop_waiting = true;
  if (message == EX_FEEDHOLD && (probe_waiting || probe_running))
    probe_cancel_waiting = true;
}
void myDwell(double value) {
  double milliseconds = MainPlanner->DwellMilli ? value : value * 1000;
  if (!std::isfinite(milliseconds) || milliseconds < 0 ||
      milliseconds > 3600000) {
    fail("Invalid dwell duration.");
    return;
  }
  dwell_waiting = static_cast<uint32_t>(std::ceil(milliseconds));
  Engine->DwellTime = dwell_waiting ? 1 : 0;
}
void myPurge(short flags) {
  if (!client.connected())
    return;
  if (!MainPlanner->ExternalStill || (flags & ~7)) {
    fail("Coordinate purge requires idle XYZ axes.");
    return;
  }
  uint8_t positions[24];
  for (unsigned i = 0; i < 3; ++i)
    sn_write_i64(positions + i * 8,
                 (flags & (1u << i)) ? 0 : Engine->Axis[i].Index);
  if (command(SN_SET_POSITION, positions, sizeof(positions))) {
    streamer.reset();
    Engine->Sync = true;
  }
}
