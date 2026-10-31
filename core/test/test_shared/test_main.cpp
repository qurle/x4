// Native unit tests for the hardware-independent logic in shared/.
// Run:  cd core && pio test -e native
#include <string.h>
#include <unity.h>

#include <string>
#include <vector>

#include "x4/config.h"
#include "x4/dither.h"
#include "x4/input.h"
#include "x4/log.h"
#include "x4/protocol.h"
#include "x4/ringlog.h"
#include "x4/safety.h"
#include "x4/shoot.h"
#include "x4/storage_layout.h"

using namespace x4;

void setUp() {}
void tearDown() {}

// ---------------------------------------------------------------- logging
static std::vector<std::string> g_lines;
static uint32_t fakeClock() { return 12345; }
static void captureSink(const char* line, size_t len, log::Level, void*) {
  g_lines.emplace_back(line, len);
}

void test_log_format_basic() {
  char buf[128];
  size_t n = log::format(buf, sizeof(buf), 12345, log::Level::Info, "core", "net", "register",
                         "cam=2 ip=192.168.4.12");
  TEST_ASSERT_EQUAL_STRING("t=12345 lvl=INFO dev=core mod=net evt=register cam=2 ip=192.168.4.12",
                           buf);
  TEST_ASSERT_EQUAL(strlen(buf), n);
}

void test_log_format_truncates_with_marker() {
  char buf[40];
  size_t n = log::format(buf, sizeof(buf), 1, log::Level::Warn, "core", "x", "y",
                         "a_very_long_tail_that_will_not_fit_in_forty_bytes");
  TEST_ASSERT_EQUAL(39, n);
  TEST_ASSERT_EQUAL('~', buf[38]);
}

void test_log_levels_and_sinks() {
  g_lines.clear();
  log::clearSinks();
  log::setClock(fakeClock);
  log::setDevice("cam3");
  log::addSink(captureSink, nullptr, log::Level::Trace);
  log::setLevel(log::Level::Warn);
  X4_LOGI("cam", "ignored", "a=%d", 1);
  X4_LOGW("cam", "kept", "a=%d", 2);
  X4_LOG_ERR(Error, "sd", "mount", Err::SD_MOUNT_FAIL, "card=%d", 0);
  TEST_ASSERT_EQUAL(2, g_lines.size());
  TEST_ASSERT_EQUAL_STRING("t=12345 lvl=WARN dev=cam3 mod=cam evt=kept a=2", g_lines[0].c_str());
  TEST_ASSERT_EQUAL_STRING(
      "t=12345 lvl=ERROR dev=cam3 mod=sd evt=mount err=300 err_name=SD_MOUNT_FAIL card=0",
      g_lines[1].c_str());
  log::setLevel(log::Level::Info);
  log::clearSinks();
}

void test_log_parse_level() {
  log::Level l;
  TEST_ASSERT_TRUE(log::parseLevel("debug", &l));
  TEST_ASSERT_EQUAL((int)log::Level::Debug, (int)l);
  TEST_ASSERT_TRUE(log::parseLevel("0", &l));
  TEST_ASSERT_EQUAL((int)log::Level::Error, (int)l);
  TEST_ASSERT_FALSE(log::parseLevel("loud", &l));
}

void test_log_rate_limit() {
  log::RateLimit rl;
  uint32_t sup = 99;
  TEST_ASSERT_TRUE(rl.allow(0, 1000, &sup));
  TEST_ASSERT_EQUAL(0, sup);
  TEST_ASSERT_FALSE(rl.allow(10, 1000, &sup));
  TEST_ASSERT_FALSE(rl.allow(500, 1000, &sup));
  TEST_ASSERT_TRUE(rl.allow(1000, 1000, &sup));
  TEST_ASSERT_EQUAL(2, sup);
}

void test_log_sanitize() {
  char out[32];
  log::sanitizeValue(out, sizeof(out), "my \"ssid\" = x");
  TEST_ASSERT_EQUAL_STRING("my_ssid__x", out);
}

void test_ringlog() {
  RingLog<3, 16> r;
  r.append("one", 3);
  r.append("two", 3);
  r.append("three", 5);
  r.append("four", 4);
  TEST_ASSERT_EQUAL(3, r.size());
  char out[64];
  r.tail(out, sizeof(out), 2);
  TEST_ASSERT_EQUAL_STRING("three\nfour\n", out);
  std::string seen;
  uint32_t last = r.forEachSince(2, [&](uint32_t, const char* t) { seen += t; seen += ","; });
  TEST_ASSERT_EQUAL_STRING("three,four,", seen.c_str());
  TEST_ASSERT_EQUAL(4, last);
  r.append("a_line_longer_than_sixteen", 26);
  r.tail(out, sizeof(out), 1);
  TEST_ASSERT_EQUAL_STRING("a_line_longer_t\n", out);
}

// ---------------------------------------------------------------- config
void test_config_defaults_valid() {
  cfg::Config c = cfg::defaults();
  cfg::Config base = cfg::defaults();
  TEST_ASSERT_EQUAL(0, cfg::validate(c, base, false, nullptr, 0));
  TEST_ASSERT_EQUAL(100, c.flash_pulse_ms);
  TEST_ASSERT_EQUAL(0, c.flash_enabled);
}

void test_config_clamps_range() {
  cfg::Config base = cfg::defaults();
  cfg::Config c = base;
  c.ap_max_clients = 2;  // below min 6
  c.log_level = 9;
  cfg::Issue is[4];
  size_t n = cfg::validate(c, base, false, is, 4);
  TEST_ASSERT_EQUAL(2, n);
  TEST_ASSERT_EQUAL(6, c.ap_max_clients);
  TEST_ASSERT_EQUAL(4, c.log_level);
  // issues come in schema order: log_level (above_max) then ap_max_clients (below_min)
  TEST_ASSERT_EQUAL_STRING("log_level", is[0].name);
  TEST_ASSERT_EQUAL_STRING("above_max", is[0].reason);
  TEST_ASSERT_EQUAL_STRING("ap_max_clients", is[1].name);
  TEST_ASSERT_EQUAL_STRING("below_min", is[1].reason);
  TEST_ASSERT_EQUAL(2, is[1].requested);
}

void test_config_safety_tighten_only() {
  cfg::Config base = cfg::defaults();
  cfg::Config c = base;
  cfg::Issue is;
  // flash pulse: may go down, never up past the compiled default (100)
  TEST_ASSERT_TRUE(cfg::set(c, base, "flash_pulse_ms", 140, &is));
  TEST_ASSERT_EQUAL(100, c.flash_pulse_ms);
  TEST_ASSERT_EQUAL_STRING("safety_tighten_only", is.reason);
  TEST_ASSERT_TRUE(cfg::set(c, base, "flash_pulse_ms", 60, &is));
  TEST_ASSERT_EQUAL(60, c.flash_pulse_ms);
  TEST_ASSERT_NULL(is.name);
  // cooldown: may go up, never down
  TEST_ASSERT_TRUE(cfg::set(c, base, "flash_cool_ms", 8000, &is));
  TEST_ASSERT_EQUAL(10000, c.flash_cool_ms);
  TEST_ASSERT_TRUE(cfg::set(c, base, "flash_cool_ms", 15000, &is));
  TEST_ASSERT_EQUAL(15000, c.flash_cool_ms);
  // non-safety options move freely inside their range
  TEST_ASSERT_TRUE(cfg::set(c, base, "xfer_parallel", 4, &is));
  TEST_ASSERT_EQUAL(4, c.xfer_parallel);
  TEST_ASSERT_FALSE(cfg::set(c, base, "no_such_key", 1, &is));
}

void test_config_schema_respects_absolute_limits() {
  const cfg::OptionInfo* p = cfg::find("flash_pulse_ms");
  TEST_ASSERT_NOT_NULL(p);
  TEST_ASSERT_TRUE(p->max <= (int32_t)safety::kFlashPulseAbsMaxMs);
  const cfg::OptionInfo* cd = cfg::find("flash_cool_ms");
  TEST_ASSERT_TRUE(cd->min >= (int32_t)safety::kFlashCooldownAbsMinMs);
  const cfg::OptionInfo* ap = cfg::find("ap_max_clients");
  TEST_ASSERT_TRUE(ap->min >= safety::kApMinClients && ap->max <= safety::kApMaxClients);
  const cfg::OptionInfo* th = cfg::find("therm_thr_c");
  TEST_ASSERT_TRUE(th->max <= safety::kThermalThrottleAbsMaxC);
  const cfg::OptionInfo* bf = cfg::find("burst_frames");
  TEST_ASSERT_TRUE(bf->max <= safety::kBurstAbsMaxFrames);
}

void test_config_summary_and_json() {
  cfg::Config c = cfg::defaults();
  char buf[2048];
  size_t n = cfg::summary(c, cfg::CAM, buf, sizeof(buf));
  TEST_ASSERT_TRUE(n > 0);
  TEST_ASSERT_NOT_NULL(strstr(buf, "cam_res=5"));
  TEST_ASSERT_NULL(strstr(buf, "flash_pulse_ms"));  // core-only option filtered out
  n = cfg::toJson(c, cfg::CORE, buf, sizeof(buf));
  TEST_ASSERT_EQUAL('{', buf[0]);
  TEST_ASSERT_EQUAL('}', buf[n - 1]);
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"flash_pulse_ms\":100"));
}

// ---------------------------------------------------------------- flash guard
void test_flash_guard() {
  safety::FlashGuard g(500, 1000);  // both clamped to absolute limits
  TEST_ASSERT_EQUAL(safety::kFlashPulseAbsMaxMs, g.cap());
  TEST_ASSERT_EQUAL(safety::kFlashCooldownAbsMinMs, g.cooldown());
  uint32_t granted, wait;
  TEST_ASSERT_EQUAL((int)safety::FlashGuard::Verdict::Disabled, (int)g.request(0, 50, &granted, &wait));
  g.setEnabled(true);
  TEST_ASSERT_EQUAL((int)safety::FlashGuard::Verdict::Ok, (int)g.request(0, 1000, &granted, &wait));
  TEST_ASSERT_EQUAL(150, granted);
  g.fired(100, granted);
  TEST_ASSERT_EQUAL((int)safety::FlashGuard::Verdict::Busy, (int)g.request(120, 50, &granted, &wait));
  TEST_ASSERT_FALSE(g.overdue(200));
  TEST_ASSERT_TRUE(g.overdue(300));
  g.ended(250);
  TEST_ASSERT_EQUAL((int)safety::FlashGuard::Verdict::Cooldown, (int)g.request(1000, 50, &granted, &wait));
  TEST_ASSERT_EQUAL(8000 - 750, wait);
  TEST_ASSERT_EQUAL((int)safety::FlashGuard::Verdict::Ok, (int)g.request(8250, 50, &granted, &wait));
  // changing limits must not reset a running cooldown
  g.fired(8250, 50);
  g.ended(8300);
  g.setLimits(100, 9000);
  TEST_ASSERT_EQUAL((int)safety::FlashGuard::Verdict::Cooldown, (int)g.request(8400, 50, &granted, &wait));
  TEST_ASSERT_EQUAL(100, g.cap());
  g.setLimits(1000, 100);  // clamped again
  TEST_ASSERT_EQUAL(safety::kFlashPulseAbsMaxMs, g.cap());
  TEST_ASSERT_EQUAL(safety::kFlashCooldownAbsMinMs, g.cooldown());
  g.setTooHot(true);
  TEST_ASSERT_EQUAL((int)safety::FlashGuard::Verdict::TooHot, (int)g.request(9000, 50, &granted, &wait));
}

// ---------------------------------------------------------------- protocol
void test_proto_shoot_roundtrip() {
  char buf[128];
  proto::ShootCmd c{42, 123456789012LL, 1, 5};
  size_t n = proto::buildShoot(buf, sizeof(buf), c);
  TEST_ASSERT_TRUE(n > 0);
  TEST_ASSERT_EQUAL_STRING("X4/1 SHOOT shot=42 at=123456789012 fl=1 res=5", buf);
  proto::KvLine kv;
  TEST_ASSERT_TRUE(kv.parse(buf, n));
  TEST_ASSERT_EQUAL(1, kv.version());
  proto::ShootCmd d;
  TEST_ASSERT_TRUE(proto::parseShoot(kv, &d));
  TEST_ASSERT_EQUAL(42, d.shot);
  TEST_ASSERT_TRUE(d.atUs == 123456789012LL);
  TEST_ASSERT_EQUAL(1, d.flash);
}

void test_proto_heartbeat_ack_roundtrip() {
  char buf[200];
  proto::Heartbeat h{3, 7, -5, 120000, 4000000, 55, 1, -61, 9};
  size_t n = proto::buildHeartbeat(buf, sizeof(buf), h);
  proto::KvLine kv;
  TEST_ASSERT_TRUE(kv.parse(buf, n));
  proto::Heartbeat g;
  TEST_ASSERT_TRUE(proto::parseHeartbeat(kv, &g));
  TEST_ASSERT_EQUAL(3, g.cam);
  TEST_ASSERT_EQUAL(-61, g.rssi);
  TEST_ASSERT_EQUAL(9, g.heldShot);

  proto::ShotAck a{2, 42, 1, 1000, 2000, 1500000, 180, 0};
  n = proto::buildShotAck(buf, sizeof(buf), a);
  TEST_ASSERT_TRUE(kv.parse(buf, n));
  proto::ShotAck b;
  TEST_ASSERT_TRUE(proto::parseShotAck(kv, &b));
  TEST_ASSERT_EQUAL(1500000, b.bytes);
  TEST_ASSERT_TRUE(b.capCoreUs == 2000);
}

void test_proto_rejects_garbage() {
  proto::KvLine kv;
  TEST_ASSERT_FALSE(kv.parse("hello", 5));
  TEST_ASSERT_FALSE(kv.parse("X4/1 NOPE a=1", 13));
  TEST_ASSERT_TRUE(kv.parse("X4/1 HB cam=9 seq=1 t1=0", 24));
  proto::Heartbeat h;
  TEST_ASSERT_FALSE(proto::parseHeartbeat(kv, &h));  // cam out of range
}

void test_proto_log_forward() {
  char buf[200];
  size_t n = proto::buildLogFwd(buf, sizeof(buf), 2, "t=5 lvl=WARN dev=cam2 mod=cam evt=hot temp=71");
  proto::KvLine kv;
  TEST_ASSERT_TRUE(kv.parse(buf, n));
  TEST_ASSERT_EQUAL((int)proto::MsgType::Log, (int)kv.type());
  TEST_ASSERT_EQUAL_STRING("2", kv.get("cam"));
  TEST_ASSERT_EQUAL_STRING("t=5 lvl=WARN dev=cam2 mod=cam evt=hot temp=71", kv.rest());
}

void test_clock_sync_min_rtt() {
  proto::ClockSync cs;
  // true offset = +1000 us. Sample A: symmetric 200 us RTT. Sample B: 5000 us RTT, asymmetric.
  cs.addSample(10000, 11100, 10200);  // off = 11100 - 10100 = 1000, rtt 200
  cs.addSample(20000, 24000, 25000);  // off = 24000 - 22500 = 1500, rtt 5000
  TEST_ASSERT_TRUE(cs.valid());
  TEST_ASSERT_TRUE(cs.offsetUs() == 1000);
  TEST_ASSERT_TRUE(cs.rttUs() == 200);
  TEST_ASSERT_TRUE(cs.toLocal(cs.toCore(777)) == 777);
}

// ---------------------------------------------------------------- input
void test_input_defaults_and_parse() {
  input::InputMap m;
  m.loadDefaults();
  TEST_ASSERT_EQUAL((int)input::Action::Shutter,
                    (int)m.lookup(input::Source::ShutterButton, input::Gesture::Press));
  int bad = 0;
  int n = m.parse("shutter.long=menu, encoder.rotate=scroll,button1.click=none", &bad);
  TEST_ASSERT_EQUAL(3, n);
  TEST_ASSERT_EQUAL(-1, bad);
  TEST_ASSERT_EQUAL((int)input::Action::Menu,
                    (int)m.lookup(input::Source::ShutterButton, input::Gesture::LongPress));
  TEST_ASSERT_EQUAL((int)input::Action::None,
                    (int)m.lookup(input::Source::Button1, input::Gesture::Click));
  n = m.parse("shutter.press=shutter,bogus.click=menu", &bad);
  TEST_ASSERT_EQUAL(1, n);
  TEST_ASSERT_EQUAL(1, bad);
  char buf[512];
  m.toString(buf, sizeof(buf));
  TEST_ASSERT_NOT_NULL(strstr(buf, "shutter.press=shutter"));
}

void test_button_press_is_instant_and_debounced() {
  input::Button b({25, 600, 0});
  using input::gestureBit;
  using input::Gesture;
  TEST_ASSERT_EQUAL(0, b.update(0, false));
  TEST_ASSERT_EQUAL(gestureBit(Gesture::Press), b.update(10, true));  // first edge
  TEST_ASSERT_EQUAL(0, b.update(12, false));                   // bounce
  TEST_ASSERT_EQUAL(0, b.update(13, true));
  TEST_ASSERT_EQUAL(0, b.update(100, false));
  uint32_t r = b.update(130, false);  // open for 30 ms -> release + click
  TEST_ASSERT_TRUE(r & gestureBit(Gesture::Release));
  TEST_ASSERT_TRUE(r & gestureBit(Gesture::Click));
  TEST_ASSERT_EQUAL(0, b.update(140, true));  // bounce right after release: ignored
  TEST_ASSERT_EQUAL(0, b.update(145, false));
}

void test_button_long_press() {
  input::Button b({25, 600, 0});
  using input::gestureBit;
  using input::Gesture;
  b.update(0, true);
  TEST_ASSERT_EQUAL(0, b.update(300, true));
  TEST_ASSERT_EQUAL(gestureBit(Gesture::LongPress), b.update(600, true));
  TEST_ASSERT_EQUAL(0, b.update(900, true));
  uint32_t r = b.update(930, false);
  r |= b.update(960, false);
  TEST_ASSERT_TRUE(r & gestureBit(Gesture::Release));
  TEST_ASSERT_FALSE(r & gestureBit(Gesture::Click));
}

void test_button_double_click() {
  input::Button b({20, 600, 300});
  using input::gestureBit;
  using input::Gesture;
  b.update(0, true);
  b.update(50, false);
  b.update(80, false);  // release, click pending
  uint32_t r = b.update(150, true);
  TEST_ASSERT_TRUE(r & gestureBit(Gesture::DoubleClick));
  b.update(200, false);
  r = b.update(230, false);
  TEST_ASSERT_FALSE(r & gestureBit(Gesture::Click));
  // single click resolves after the window
  b.update(1000, true);
  b.update(1050, false);
  b.update(1080, false);
  r = b.update(1400, false);
  TEST_ASSERT_TRUE(r & gestureBit(Gesture::Click));
}

void test_swipe_detector() {
  input::SwipeDetector s;
  TEST_ASSERT_EQUAL((int)input::Gesture::kCount, (int)s.update(0, true, 200, 100));
  s.update(50, true, 120, 105);
  TEST_ASSERT_EQUAL((int)input::Gesture::SwipeLeft, (int)s.update(100, false, 0, 0));
  s.update(200, true, 100, 100);
  TEST_ASSERT_EQUAL((int)input::Gesture::kCount, (int)s.update(250, false, 0, 0));  // tap
}

// ---------------------------------------------------------------- shooting FSM
static void allOnline(shoot::Controller& c, int n) {
  bool on[4] = {n > 0, n > 1, n > 2, n > 3};
  c.setCameras(on, 0);
}

static proto::ShotAck ack(uint8_t cam, uint32_t shot, bool ok, int64_t cap) {
  return proto::ShotAck{cam, shot, (uint8_t)ok, cap - 50, cap, 1000000, 150, 0};
}

void test_fsm_boot_to_armed() {
  shoot::Controller c;
  TEST_ASSERT_EQUAL((int)shoot::State::Booting, (int)c.state());
  allOnline(c, 4);
  TEST_ASSERT_EQUAL((int)shoot::State::Booting, (int)c.state());
  uint32_t e = c.bootDone(10);
  TEST_ASSERT_TRUE(e & shoot::kStateChanged);
  TEST_ASSERT_EQUAL((int)shoot::State::Armed, (int)c.state());
  c.setPreviewWanted(true, 20);
  TEST_ASSERT_EQUAL((int)shoot::State::Preview, (int)c.state());
}

void test_fsm_full_shot() {
  shoot::Params p;
  p.parallel = 2;
  shoot::Controller c(p);
  allOnline(c, 4);
  c.bootDone(0);
  c.setPreviewWanted(true, 0);
  uint32_t e = c.shutter(1000, 1000000, 7);
  TEST_ASSERT_TRUE(e & shoot::kSendTrigger);
  TEST_ASSERT_TRUE(e & shoot::kPausePreview);
  TEST_ASSERT_EQUAL((int)shoot::State::Shooting, (int)c.state());
  TEST_ASSERT_TRUE(c.session().atUs == 1000000 + 120000);
  // shutter again while busy is refused
  TEST_ASSERT_EQUAL(shoot::kShutterRefused, c.shutter(1001, 1001000, 8));
  for (uint8_t i = 1; i <= 4; i++) e = c.onAck(ack(i, 7, true, 1120000 + i * 1000), 1200 + i);
  TEST_ASSERT_TRUE(e & shoot::kShotCaptured);
  TEST_ASSERT_EQUAL((int)shoot::State::Transferring, (int)c.state());
  TEST_ASSERT_TRUE(c.session().skewUs() == 3000);
  uint8_t a = c.nextToFetch(), b = c.nextToFetch();
  TEST_ASSERT_EQUAL(1, a);
  TEST_ASSERT_EQUAL(2, b);
  TEST_ASSERT_EQUAL(0, c.nextToFetch());  // parallel limit
  c.onStored(1, true, 2000);
  TEST_ASSERT_EQUAL(3, c.nextToFetch());
  c.onStored(2, true, 2500);
  TEST_ASSERT_EQUAL(4, c.nextToFetch());
  c.onStored(3, true, 3000);
  e = c.onStored(4, true, 3500);
  TEST_ASSERT_TRUE(e & shoot::kShotStored);
  TEST_ASSERT_FALSE(e & shoot::kShotPartial);
  TEST_ASSERT_FALSE(e & shoot::kOverBudget);
  TEST_ASSERT_TRUE(e & shoot::kResumePreview);
  TEST_ASSERT_EQUAL((int)shoot::State::Preview, (int)c.state());
  TEST_ASSERT_EQUAL(2500, c.session().totalMs());
}

void test_fsm_ack_timeout_and_partial() {
  shoot::Controller c;
  allOnline(c, 3);
  c.bootDone(0);
  c.shutter(0, 0, 1);
  c.onAck(ack(1, 1, true, 100), 200);
  c.onAck(ack(2, 1, false, 0), 210);
  TEST_ASSERT_EQUAL((int)shoot::State::Shooting, (int)c.state());
  uint32_t e = c.tick(5000);  // cam 3 never answered
  TEST_ASSERT_TRUE(e & shoot::kShotCaptured);
  TEST_ASSERT_EQUAL(1, c.nextToFetch());
  TEST_ASSERT_EQUAL(0, c.nextToFetch());
  e = c.onStored(1, true, 5100);
  TEST_ASSERT_TRUE(e & shoot::kShotStored);
  TEST_ASSERT_TRUE(e & shoot::kShotPartial);
  TEST_ASSERT_TRUE(e & shoot::kOverBudget);
  TEST_ASSERT_EQUAL((int)shoot::State::Armed, (int)c.state());
}

void test_fsm_transfer_retries() {
  shoot::Params p;
  p.retries = 1;
  shoot::Controller c(p);
  allOnline(c, 1);
  c.bootDone(0);
  c.shutter(0, 0, 1);
  c.onAck(ack(1, 1, true, 100), 200);
  TEST_ASSERT_EQUAL(1, c.nextToFetch());
  c.onTransferRetry(1);
  TEST_ASSERT_EQUAL(1, c.nextToFetch());
  uint32_t e = c.onTransferRetry(1);  // second failure > retries -> failed
  TEST_ASSERT_TRUE(e & shoot::kShotStored);
  TEST_ASSERT_TRUE(e & shoot::kShotPartial);
}

void test_fsm_faults() {
  shoot::Controller c;
  allOnline(c, 4);
  c.bootDone(0);
  c.setFault(shoot::Fault::SdMissing, true, 1);
  TEST_ASSERT_EQUAL((int)shoot::State::Error, (int)c.state());
  TEST_ASSERT_EQUAL(shoot::kShutterRefused, c.shutter(2, 2000, 1));
  c.setFault(shoot::Fault::SdMissing, false, 3);
  TEST_ASSERT_EQUAL((int)shoot::State::Armed, (int)c.state());
  bool none[4] = {false, false, false, false};
  c.setCameras(none, 4);
  TEST_ASSERT_EQUAL((int)shoot::State::Idle, (int)c.state());
  TEST_ASSERT_TRUE(c.hasFault(shoot::Fault::NoCameras));
}

void test_fsm_ignores_stale_and_duplicate_acks() {
  shoot::Controller c;
  allOnline(c, 2);
  c.bootDone(0);
  c.shutter(0, 0, 5);
  TEST_ASSERT_EQUAL(shoot::kNone, c.onAck(ack(1, 4, true, 1), 10));  // old shot id
  c.onAck(ack(1, 5, true, 1), 10);
  TEST_ASSERT_EQUAL(shoot::kNone, c.onAck(ack(1, 5, true, 1), 11));  // duplicate
  TEST_ASSERT_EQUAL(1, c.session().acked());
}

// ---------------------------------------------------------------- storage + dither
void test_storage_paths_and_sidecar() {
  char buf[64];
  storage::camFile(buf, sizeof(buf), 42, 3);
  TEST_ASSERT_EQUAL_STRING("/x4/shots/S000042/cam3.jpg", buf);
  storage::sidecarFile(buf, sizeof(buf), 42);
  TEST_ASSERT_EQUAL_STRING("/x4/shots/S000042/shot.json", buf);

  shoot::Session s;
  s.id = 42;
  s.cam[0].expected = s.cam[0].ok = s.cam[0].stored = true;
  s.cam[0].bytes = 1234;
  char js[1024];
  storage::SidecarInfo info{&s, "0.1.0", "abc", "", 5, 1, false, 50};
  size_t n = storage::sidecarJson(js, sizeof(js), info);
  TEST_ASSERT_TRUE(n > 0);
  TEST_ASSERT_NOT_NULL(strstr(js, "\"shot\":42"));
  TEST_ASSERT_NOT_NULL(strstr(js, "\"file\":\"cam1.jpg\""));
  TEST_ASSERT_EQUAL(0, storage::sidecarJson(js, 20, info));  // overflow reported
}

void test_dither() {
  uint8_t gray[16 * 2];
  memset(gray, 0, sizeof(gray));
  memset(gray + 16, 255, 16);
  uint8_t mono[4];
  dither::toMono(gray, 16, 2, mono);
  TEST_ASSERT_EQUAL_HEX8(0x00, mono[0]);
  TEST_ASSERT_EQUAL_HEX8(0xFF, mono[2]);
  uint8_t mid[64];
  memset(mid, 128, sizeof(mid));
  uint8_t m[8];
  dither::toMono(mid, 8, 8, m);
  int ones = 0;
  for (uint8_t b : m)
    for (int i = 0; i < 8; i++) ones += (b >> i) & 1;
  TEST_ASSERT_INT_WITHIN(2, 32, ones);  // ~50 % coverage
  uint8_t hdr[62];
  TEST_ASSERT_EQUAL(62, dither::bmpMonoHeader(hdr, 16, 2));
  TEST_ASSERT_EQUAL('B', hdr[0]);
  TEST_ASSERT_EQUAL(4, dither::bmpMonoRowBytes(16));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_log_format_basic);
  RUN_TEST(test_log_format_truncates_with_marker);
  RUN_TEST(test_log_levels_and_sinks);
  RUN_TEST(test_log_parse_level);
  RUN_TEST(test_log_rate_limit);
  RUN_TEST(test_log_sanitize);
  RUN_TEST(test_ringlog);
  RUN_TEST(test_config_defaults_valid);
  RUN_TEST(test_config_clamps_range);
  RUN_TEST(test_config_safety_tighten_only);
  RUN_TEST(test_config_schema_respects_absolute_limits);
  RUN_TEST(test_config_summary_and_json);
  RUN_TEST(test_flash_guard);
  RUN_TEST(test_proto_shoot_roundtrip);
  RUN_TEST(test_proto_heartbeat_ack_roundtrip);
  RUN_TEST(test_proto_rejects_garbage);
  RUN_TEST(test_proto_log_forward);
  RUN_TEST(test_clock_sync_min_rtt);
  RUN_TEST(test_input_defaults_and_parse);
  RUN_TEST(test_button_press_is_instant_and_debounced);
  RUN_TEST(test_button_long_press);
  RUN_TEST(test_button_double_click);
  RUN_TEST(test_swipe_detector);
  RUN_TEST(test_fsm_boot_to_armed);
  RUN_TEST(test_fsm_full_shot);
  RUN_TEST(test_fsm_ack_timeout_and_partial);
  RUN_TEST(test_fsm_transfer_retries);
  RUN_TEST(test_fsm_faults);
  RUN_TEST(test_fsm_ignores_stale_and_duplicate_acks);
  RUN_TEST(test_storage_paths_and_sidecar);
  RUN_TEST(test_dither);
  return UNITY_END();
}
