// x4 wire protocol shared by the Core and the cameras. Full spec: docs/PROTOCOL.md.
//
// UDP datagrams are single-line text in the same key=value style as the logs:
//   X4/1 SHOOT shot=42 at=123456789 fl=0 res=5
//   ^magic/version  ^type  ^fields
// Text keeps them readable in Wireshark and in the serial log, and costs <100 bytes.
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "version.h"

namespace x4 {
namespace proto {

// ---- addressing ------------------------------------------------------------------------------
constexpr const char* kApIp = "192.168.4.1";  // Core SoftAP address (ESP32 default)
constexpr uint16_t kUdpCamPort = 4210;          // Core -> cameras (SHOOT, HBA, CMD)
constexpr uint16_t kUdpCorePort = 4211;         // cameras -> Core (HB, ACK, LOG)
constexpr uint16_t kCamHttpPort = 80;           // /capture /settings /info /health /shot /logs
constexpr uint16_t kCamStreamPort = 81;         // MJPEG /stream
constexpr uint16_t kCoreHttpPort = 80;          // web debug page + API + /register
constexpr uint8_t kMaxCams = 4;

// HTTP paths
constexpr const char* kPathRegister = "/register";  // camera -> Core, POST JSON
constexpr const char* kPathInfo = "/info";
constexpr const char* kPathHealth = "/health";
constexpr const char* kPathCapture = "/capture";
constexpr const char* kPathSettings = "/settings";
constexpr const char* kPathShot = "/shot";  // GET /shot?id=42 -> held JPEG
constexpr const char* kPathStream = "/stream";
constexpr const char* kPathLogs = "/logs";

enum class MsgType : uint8_t {
  Unknown = 0,
  Shoot,      // Core -> cam: capture shot `shot` at Core time `at` (us, 0 = now)
  HbAck,      // Core -> cam: heartbeat answer with Core time (clock sync)
  Cmd,        // Core -> cam: small command (preview on/off, reboot, setcfg)
  Heartbeat,  // cam -> Core: alive + stats + clock-sync request
  Ack,        // cam -> Core: shot captured (or failed)
  Log,        // cam -> Core: forwarded log line (WARN+)
};

const char* msgTypeName(MsgType t);

// ---- tiny key=value parser for one datagram / log line -----------------------------------------
class KvLine {
 public:
  // Parses "X4/1 TYPE k=v k=v". Keeps pointers into an internal copy; max 192 chars, 16 keys.
  bool parse(const char* line, size_t len);
  MsgType type() const { return type_; }
  int version() const { return version_; }
  const char* typeStr() const { return typeStr_; }
  const char* get(const char* key) const;  // nullptr if absent
  bool getI64(const char* key, int64_t* out) const;
  bool getU32(const char* key, uint32_t* out) const;
  int64_t i64(const char* key, int64_t def) const;
  size_t count() const { return n_; }
  // For LOG messages: the text after "LOG cam=N ".
  const char* rest() const { return rest_; }

 private:
  char buf_[196];
  const char* keys_[16];
  const char* vals_[16];
  size_t n_ = 0;
  int version_ = 0;
  MsgType type_ = MsgType::Unknown;
  const char* typeStr_ = "";
  const char* rest_ = "";
};

// ---- message builders (return length, 0 on overflow) ------------------------------------------
struct ShootCmd {
  uint32_t shot;   // monotonically increasing per Core boot (+ boot counter in high bits)
  int64_t atUs;    // Core time to capture at (esp_timer us). 0 = immediately on receipt.
  uint8_t flash;   // 1 if the Core will fire the flash around atUs (cameras may lock exposure)
  uint8_t res;     // cfg cam_res value to use for this shot
};
size_t buildShoot(char* buf, size_t cap, const ShootCmd& c);
bool parseShoot(const KvLine& kv, ShootCmd* out);

struct Heartbeat {
  uint8_t cam;
  uint32_t seq;
  int64_t t1Us;  // camera clock when sent
  uint32_t heap;
  uint32_t psram;
  int16_t tempC;
  uint8_t state;  // cam_state_t
  int8_t rssi;
  uint32_t heldShot;  // shot id held in PSRAM (0 = none)
};
size_t buildHeartbeat(char* buf, size_t cap, const Heartbeat& h);
bool parseHeartbeat(const KvLine& kv, Heartbeat* out);

struct HbAck {
  uint32_t seq;
  int64_t t1Us;  // echoed
  int64_t t2Us;  // Core clock when received
};
size_t buildHbAck(char* buf, size_t cap, const HbAck& a);
bool parseHbAck(const KvLine& kv, HbAck* out);

struct ShotAck {
  uint8_t cam;
  uint32_t shot;
  uint8_t ok;
  int64_t rxCoreUs;   // when the camera got the SHOOT, in Core time
  int64_t capCoreUs;  // frame timestamp, in Core time
  uint32_t bytes;
  uint32_t capMs;     // fb_get duration
  uint16_t err;       // x4::Err when ok=0
};
size_t buildShotAck(char* buf, size_t cap, const ShotAck& a);
bool parseShotAck(const KvLine& kv, ShotAck* out);

size_t buildLogFwd(char* buf, size_t cap, uint8_t cam, const char* line);
size_t buildCmd(char* buf, size_t cap, const char* cmd, const char* arg);

// ---- clock sync (NTP-style, min-RTT filter) ---------------------------------------------------
// Camera sends t1 (local), Core stamps t2 (Core) on receipt, camera stamps t3 (local) on reply.
// offset = t2 - (t1 + t3)/2 ; error <= rtt/2. We keep the sample with the lowest RTT among the
// last kWindow samples, so a few slow packets do not disturb the estimate.
class ClockSync {
 public:
  static constexpr int kWindow = 8;
  void addSample(int64_t t1, int64_t t2, int64_t t3);
  bool valid() const { return valid_; }
  int64_t offsetUs() const { return offset_; }  // core = local + offset
  int64_t rttUs() const { return rtt_; }
  int64_t toCore(int64_t localUs) const { return localUs + offset_; }
  int64_t toLocal(int64_t coreUs) const { return coreUs - offset_; }
  void reset() { *this = ClockSync(); }

 private:
  struct S {
    int64_t offset;
    int64_t rtt;
  };
  S s_[kWindow] = {};
  int n_ = 0;
  int next_ = 0;
  bool valid_ = false;
  int64_t offset_ = 0;
  int64_t rtt_ = 0;
};

// Camera states reported in heartbeats.
enum class CamState : uint8_t { Booting = 0, Idle, Armed, Capturing, Holding, Streaming, Error };
const char* camStateName(CamState s);

}  // namespace proto
}  // namespace x4
