#include "x4/protocol.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace x4 {
namespace proto {
namespace {

struct TypeName {
  MsgType t;
  const char* s;
};
const TypeName kTypes[] = {{MsgType::Shoot, "SHOOT"}, {MsgType::HbAck, "HBA"},
                           {MsgType::Cmd, "CMD"},     {MsgType::Heartbeat, "HB"},
                           {MsgType::Ack, "ACK"},     {MsgType::Log, "LOG"}};

size_t fin(int n, size_t cap) { return (n < 0 || (size_t)n >= cap) ? 0 : (size_t)n; }

}  // namespace

const char* msgTypeName(MsgType t) {
  for (const auto& e : kTypes)
    if (e.t == t) return e.s;
  return "?";
}

bool KvLine::parse(const char* line, size_t len) {
  n_ = 0;
  version_ = 0;
  type_ = MsgType::Unknown;
  typeStr_ = "";
  rest_ = "";
  if (!line) return false;
  if (len >= sizeof(buf_)) len = sizeof(buf_) - 1;
  memcpy(buf_, line, len);
  buf_[len] = 0;
  // trim trailing newline
  while (len && (buf_[len - 1] == '\n' || buf_[len - 1] == '\r')) buf_[--len] = 0;
  if (strncmp(buf_, "X4/", 3) != 0) return false;
  char* p = buf_ + 3;
  version_ = (int)strtol(p, &p, 10);
  if (*p != ' ') return false;
  *p++ = 0;
  typeStr_ = p;
  while (*p && *p != ' ') p++;
  if (*p) *p++ = 0;
  for (const auto& e : kTypes)
    if (strcmp(typeStr_, e.s) == 0) type_ = e.t;
  const char* restStart = p;
  // LOG carries a free-form log line after cam=N; keep it intact.
  bool isLog = type_ == MsgType::Log;
  while (*p && n_ < 16) {
    while (*p == ' ') p++;
    if (!*p) break;
    char* k = p;
    while (*p && *p != '=' && *p != ' ') p++;
    if (*p != '=') {  // token without '=': stop (e.g. LOG payload)
      rest_ = k;
      break;
    }
    *p++ = 0;
    char* v = p;
    while (*p && *p != ' ') p++;
    bool more = *p != 0;
    if (more) *p++ = 0;
    keys_[n_] = k;
    vals_[n_] = v;
    n_++;
    if (isLog && strcmp(k, "cam") == 0) {
      rest_ = more ? p : "";
      break;
    }
  }
  (void)restStart;
  return type_ != MsgType::Unknown;
}

const char* KvLine::get(const char* key) const {
  for (size_t i = 0; i < n_; i++)
    if (strcmp(keys_[i], key) == 0) return vals_[i];
  return nullptr;
}

bool KvLine::getI64(const char* key, int64_t* out) const {
  const char* v = get(key);
  if (!v || !*v) return false;
  char* end = nullptr;
  long long x = strtoll(v, &end, 10);
  if (end == v || *end) return false;
  *out = (int64_t)x;
  return true;
}

bool KvLine::getU32(const char* key, uint32_t* out) const {
  int64_t v;
  if (!getI64(key, &v) || v < 0 || v > 0xFFFFFFFFLL) return false;
  *out = (uint32_t)v;
  return true;
}

int64_t KvLine::i64(const char* key, int64_t def) const {
  int64_t v;
  return getI64(key, &v) ? v : def;
}

size_t buildShoot(char* buf, size_t cap, const ShootCmd& c) {
  return fin(snprintf(buf, cap, "X4/%d SHOOT shot=%" PRIu32 " at=%" PRId64 " fl=%u res=%u dl=%" PRIu32,
                      X4_PROTOCOL_VERSION, c.shot, c.atUs, (unsigned)c.flash, (unsigned)c.res, c.dlUs),
             cap);
}

bool parseShoot(const KvLine& kv, ShootCmd* out) {
  if (kv.type() != MsgType::Shoot) return false;
  ShootCmd c{};
  if (!kv.getU32("shot", &c.shot)) return false;
  c.atUs = kv.i64("at", 0);
  c.flash = (uint8_t)kv.i64("fl", 0);
  c.res = (uint8_t)kv.i64("res", 5);
  c.dlUs = (uint32_t)kv.i64("dl", 0);
  *out = c;
  return true;
}

size_t buildHeartbeat(char* buf, size_t cap, const Heartbeat& h) {
  return fin(snprintf(buf, cap,
                      "X4/%d HB cam=%u seq=%" PRIu32 " t1=%" PRId64 " heap=%" PRIu32
                      " psram=%" PRIu32 " temp=%d st=%u rssi=%d held=%" PRIu32 " sw=%u",
                      X4_PROTOCOL_VERSION, (unsigned)h.cam, h.seq, h.t1Us, h.heap, h.psram,
                      (int)h.tempC, (unsigned)h.state, (int)h.rssi, h.heldShot, (unsigned)h.switchMs),
             cap);
}

bool parseHeartbeat(const KvLine& kv, Heartbeat* out) {
  if (kv.type() != MsgType::Heartbeat) return false;
  Heartbeat h{};
  int64_t cam;
  if (!kv.getI64("cam", &cam) || cam < 1 || cam > kMaxCams) return false;
  h.cam = (uint8_t)cam;
  if (!kv.getU32("seq", &h.seq)) return false;
  if (!kv.getI64("t1", &h.t1Us)) return false;
  h.heap = (uint32_t)kv.i64("heap", 0);
  h.psram = (uint32_t)kv.i64("psram", 0);
  h.tempC = (int16_t)kv.i64("temp", 0);
  h.state = (uint8_t)kv.i64("st", 0);
  h.rssi = (int8_t)kv.i64("rssi", 0);
  h.heldShot = (uint32_t)kv.i64("held", 0);
  h.switchMs = (uint16_t)kv.i64("sw", 0);
  *out = h;
  return true;
}

size_t buildHbAck(char* buf, size_t cap, const HbAck& a) {
  return fin(snprintf(buf, cap, "X4/%d HBA seq=%" PRIu32 " t1=%" PRId64 " t2=%" PRId64,
                      X4_PROTOCOL_VERSION, a.seq, a.t1Us, a.t2Us),
             cap);
}

bool parseHbAck(const KvLine& kv, HbAck* out) {
  if (kv.type() != MsgType::HbAck) return false;
  HbAck a{};
  if (!kv.getU32("seq", &a.seq) || !kv.getI64("t1", &a.t1Us) || !kv.getI64("t2", &a.t2Us))
    return false;
  *out = a;
  return true;
}

size_t buildShotAck(char* buf, size_t cap, const ShotAck& a) {
  return fin(snprintf(buf, cap,
                      "X4/%d ACK cam=%u shot=%" PRIu32 " ok=%u rx=%" PRId64 " cap=%" PRId64
                      " bytes=%" PRIu32 " cap_ms=%" PRIu32 " err=%u",
                      X4_PROTOCOL_VERSION, (unsigned)a.cam, a.shot, (unsigned)a.ok, a.rxCoreUs,
                      a.capCoreUs, a.bytes, a.capMs, (unsigned)a.err),
             cap);
}

bool parseShotAck(const KvLine& kv, ShotAck* out) {
  if (kv.type() != MsgType::Ack) return false;
  ShotAck a{};
  int64_t cam;
  if (!kv.getI64("cam", &cam) || cam < 1 || cam > kMaxCams) return false;
  a.cam = (uint8_t)cam;
  if (!kv.getU32("shot", &a.shot)) return false;
  a.ok = (uint8_t)kv.i64("ok", 0);
  a.rxCoreUs = kv.i64("rx", 0);
  a.capCoreUs = kv.i64("cap", 0);
  a.bytes = (uint32_t)kv.i64("bytes", 0);
  a.capMs = (uint32_t)kv.i64("cap_ms", 0);
  a.err = (uint16_t)kv.i64("err", 0);
  *out = a;
  return true;
}

size_t buildLogFwd(char* buf, size_t cap, uint8_t cam, const char* line) {
  return fin(snprintf(buf, cap, "X4/%d LOG cam=%u %s", X4_PROTOCOL_VERSION, (unsigned)cam,
                      line ? line : ""),
             cap);
}

size_t buildCmd(char* buf, size_t cap, const char* cmd, const char* arg) {
  return fin(snprintf(buf, cap, "X4/%d CMD cmd=%s%s%s", X4_PROTOCOL_VERSION, cmd ? cmd : "",
                      arg && *arg ? " " : "", arg ? arg : ""),
             cap);
}

void ClockSync::addSample(int64_t t1, int64_t t2, int64_t t3) {
  int64_t rtt = t3 - t1;
  if (rtt < 0) return;  // clock went backwards or garbage
  int64_t off = t2 - (t1 + t3) / 2;
  s_[next_] = S{off, rtt};
  next_ = (next_ + 1) % kWindow;
  if (n_ < kWindow) n_++;
  int best = 0;
  for (int i = 1; i < n_; i++)
    if (s_[i].rtt < s_[best].rtt) best = i;
  offset_ = s_[best].offset;
  rtt_ = s_[best].rtt;
  valid_ = true;
}

const char* camStateName(CamState s) {
  switch (s) {
    case CamState::Booting: return "booting";
    case CamState::Idle: return "idle";
    case CamState::Armed: return "armed";
    case CamState::Capturing: return "capturing";
    case CamState::Holding: return "holding";
    case CamState::Streaming: return "streaming";
    case CamState::Error: return "error";
  }
  return "?";
}

}  // namespace proto
}  // namespace x4
