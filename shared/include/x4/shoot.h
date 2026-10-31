// Shooting flow state machine + per-shot bookkeeping. Pure logic (no WiFi, no SD, no LVGL), so
// it is unit-tested natively. core/src/app/ owns one instance and turns Effects into real work.
//
//   Booting -> Idle  (until the minimum number of cameras is online and the SD is mounted)
//   Idle    -> Armed (ready)            Armed <-> Preview (live preview running)
//   Armed/Preview --shutter--> Shooting --all ACKs/timeout--> Transferring --all stored--> Armed
//   any --blocking fault (SD missing/full, overheat, no cameras)--> Error --fault cleared--> Idle
// See docs/FIRMWARE.md#state-machine for the full table.
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "protocol.h"

namespace x4 {
namespace shoot {

enum class State : uint8_t { Booting = 0, Idle, Armed, Preview, Shooting, Transferring, Error };
const char* stateName(State s);

enum class Fault : uint8_t {
  None = 0,
  NoCameras = 1 << 0,
  SdMissing = 1 << 1,
  SdFull = 1 << 2,
  Overheat = 1 << 3,
};

// Effects returned by the controller; the app performs them.
enum Effect : uint32_t {
  kNone = 0,
  kStateChanged = 1u << 0,
  kSendTrigger = 1u << 1,      // send SHOOT for session().id at session().atUs
  kFireFlash = 1u << 2,        // fire flash around atUs (only if flash armed for this shot)
  kPausePreview = 1u << 3,
  kResumePreview = 1u << 4,
  kShotCaptured = 1u << 5,     // every expected camera ACKed: UI "captured" moment
  kStartTransfer = 1u << 6,    // begin/continue pulling photos (see nextToFetch)
  kShotStored = 1u << 7,       // all photos stored (or failed) -> write sidecar
  kShotPartial = 1u << 8,      // finished but some cameras failed
  kShutterRefused = 1u << 9,   // shutter pressed while not ready (UI: shake)
  kOverBudget = 1u << 10,      // finished later than shot_budget_ms
};

struct CamShot {
  bool expected = false;  // camera was online when the shot was triggered
  bool acked = false;
  bool ok = false;        // captured successfully
  bool stored = false;
  bool failed = false;    // capture failed, ACK missing or transfer gave up
  bool inFlight = false;  // transfer running
  uint8_t tries = 0;
  int64_t rxCoreUs = 0;
  int64_t capCoreUs = 0;
  uint32_t bytes = 0;
  uint32_t capMs = 0;
  uint32_t ackAtMs = 0;
  uint32_t storedAtMs = 0;
};

struct Session {
  uint32_t id = 0;
  uint32_t t0Ms = 0;  // shutter press (Core ms)
  int64_t atUs = 0;   // scheduled capture time (Core us)
  bool flash = false;
  CamShot cam[proto::kMaxCams];
  uint32_t capturedAtMs = 0;
  uint32_t doneAtMs = 0;

  int expected() const;
  int acked() const;
  int ok() const;
  int stored() const;
  int failed() const;
  bool allAcked() const;   // every expected cam ACKed or failed
  bool settled() const;    // every expected cam stored or failed
  // Spread of capture timestamps among successful cameras (us). 0 if fewer than 2.
  int64_t skewUs() const;
  uint32_t totalMs() const { return doneAtMs ? doneAtMs - t0Ms : 0; }
};

struct Params {
  uint32_t ackTimeoutMs = 1500;
  uint32_t leadMs = 120;
  uint32_t budgetMs = 5000;
  uint8_t parallel = 2;
  uint8_t retries = 2;
  uint8_t minCams = 1;
  uint32_t camDelayMs = 0;  // sequential capture: the last camera fires 3 x this after the first
};

class Controller {
 public:
  explicit Controller(const Params& p = Params()) : p_(p) {}
  void setParams(const Params& p) { p_ = p; }
  const Params& params() const { return p_; }

  State state() const { return state_; }
  const Session& session() const { return s_; }
  uint8_t faults() const { return faults_; }
  bool hasFault(Fault f) const { return faults_ & (uint8_t)f; }

  // ---- inputs: environment ----
  // online[i] = camera i+1 online. Call whenever it changes (cheap to call every tick).
  uint32_t setCameras(const bool online[proto::kMaxCams], uint32_t nowMs);
  uint32_t setFault(Fault f, bool on, uint32_t nowMs);
  uint32_t setPreviewWanted(bool on, uint32_t nowMs);
  void setFlashArmed(bool on) { flashArmed_ = on; }  // flash allowed+ready for the next shot
  uint32_t bootDone(uint32_t nowMs);

  // ---- inputs: user / network ----
  // nowUs = Core esp_timer time; shotId chosen by the caller (persistent counter).
  uint32_t shutter(uint32_t nowMs, int64_t nowUs, uint32_t shotId);
  uint32_t onAck(const proto::ShotAck& a, uint32_t nowMs);
  uint32_t onStored(uint8_t cam, bool ok, uint32_t nowMs);  // transfer finished (ok or gave up)
  uint32_t onTransferRetry(uint8_t cam);                    // a transfer attempt failed, will retry
  uint32_t tick(uint32_t nowMs);

  // Next camera (1..4) to download, honouring the parallel limit; 0 if none. Marks it in flight.
  uint8_t nextToFetch();

 private:
  uint32_t enter(State s, uint32_t nowMs);
  uint32_t reevaluate(uint32_t nowMs);
  uint32_t finishIfSettled(uint32_t nowMs);
  int onlineCount() const;

  Params p_;
  State state_ = State::Booting;
  Session s_;
  bool online_[proto::kMaxCams] = {};
  uint8_t faults_ = 0;
  bool previewWanted_ = false;
  bool flashArmed_ = false;
  bool booted_ = false;
};

}  // namespace shoot
}  // namespace x4
