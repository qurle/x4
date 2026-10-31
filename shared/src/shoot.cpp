#include "x4/shoot.h"

namespace x4 {
namespace shoot {

const char* stateName(State s) {
  switch (s) {
    case State::Booting: return "booting";
    case State::Idle: return "idle";
    case State::Armed: return "armed";
    case State::Preview: return "preview";
    case State::Shooting: return "shooting";
    case State::Transferring: return "transferring";
    case State::Error: return "error";
  }
  return "?";
}

#define X4_COUNT(cond)                                         \
  int n = 0;                                                   \
  for (const auto& c : cam)                                    \
    if (c.expected && (cond)) n++;                             \
  return n

int Session::expected() const { X4_COUNT(true); }
int Session::acked() const { X4_COUNT(c.acked); }
int Session::ok() const { X4_COUNT(c.ok); }
int Session::stored() const { X4_COUNT(c.stored); }
int Session::failed() const { X4_COUNT(c.failed); }
#undef X4_COUNT

bool Session::allAcked() const {
  for (const auto& c : cam)
    if (c.expected && !c.acked && !c.failed) return false;
  return true;
}

bool Session::settled() const {
  for (const auto& c : cam)
    if (c.expected && !c.stored && !c.failed) return false;
  return true;
}

int64_t Session::skewUs() const {
  int64_t lo = 0, hi = 0;
  int n = 0;
  for (const auto& c : cam) {
    if (!c.expected || !c.ok) continue;
    if (n == 0 || c.capCoreUs < lo) lo = c.capCoreUs;
    if (n == 0 || c.capCoreUs > hi) hi = c.capCoreUs;
    n++;
  }
  return n < 2 ? 0 : hi - lo;
}

int Controller::onlineCount() const {
  int n = 0;
  for (bool o : online_) n += o ? 1 : 0;
  return n;
}

uint32_t Controller::enter(State s, uint32_t) {
  if (s == state_) return kNone;
  uint32_t eff = kStateChanged;
  State prev = state_;
  state_ = s;
  if (prev == State::Preview && s != State::Preview) eff |= kPausePreview;
  if (s == State::Preview) eff |= kResumePreview;
  return eff;
}

// Decides the resting state when not busy with a shot.
uint32_t Controller::reevaluate(uint32_t nowMs) {
  if (state_ == State::Shooting || state_ == State::Transferring) return kNone;
  if (!booted_) return enter(State::Booting, nowMs);
  if (faults_ & ((uint8_t)Fault::SdMissing | (uint8_t)Fault::SdFull | (uint8_t)Fault::Overheat))
    return enter(State::Error, nowMs);
  if (onlineCount() < p_.minCams) {
    // Not an "error" during normal start-up: cameras take a few seconds to join.
    return enter(State::Idle, nowMs);
  }
  return enter(previewWanted_ ? State::Preview : State::Armed, nowMs);
}

uint32_t Controller::bootDone(uint32_t nowMs) {
  booted_ = true;
  return reevaluate(nowMs);
}

uint32_t Controller::setCameras(const bool online[proto::kMaxCams], uint32_t nowMs) {
  bool changed = false;
  for (int i = 0; i < proto::kMaxCams; i++) {
    if (online_[i] != online[i]) changed = true;
    online_[i] = online[i];
  }
  uint8_t before = faults_;
  if (onlineCount() == 0)
    faults_ |= (uint8_t)Fault::NoCameras;
  else
    faults_ &= ~(uint8_t)Fault::NoCameras;
  if (!changed && before == faults_) return kNone;
  return reevaluate(nowMs);
}

uint32_t Controller::setFault(Fault f, bool on, uint32_t nowMs) {
  uint8_t before = faults_;
  if (on)
    faults_ |= (uint8_t)f;
  else
    faults_ &= ~(uint8_t)f;
  if (before == faults_) return kNone;
  return reevaluate(nowMs);
}

uint32_t Controller::setPreviewWanted(bool on, uint32_t nowMs) {
  previewWanted_ = on;
  return reevaluate(nowMs);
}

uint32_t Controller::shutter(uint32_t nowMs, int64_t nowUs, uint32_t shotId) {
  if (state_ != State::Armed && state_ != State::Preview) return kShutterRefused;
  s_ = Session();
  s_.id = shotId;
  s_.t0Ms = nowMs;
  s_.atUs = p_.leadMs ? nowUs + (int64_t)p_.leadMs * 1000 : 0;
  s_.flash = flashArmed_;
  for (int i = 0; i < proto::kMaxCams; i++) s_.cam[i].expected = online_[i];
  uint32_t eff = enter(State::Shooting, nowMs) | kSendTrigger;
  if (s_.flash) eff |= kFireFlash;
  return eff;
}

uint32_t Controller::onAck(const proto::ShotAck& a, uint32_t nowMs) {
  if (state_ != State::Shooting && state_ != State::Transferring) return kNone;
  if (a.shot != s_.id || a.cam < 1 || a.cam > proto::kMaxCams) return kNone;
  CamShot& c = s_.cam[a.cam - 1];
  if (!c.expected || c.acked) return kNone;  // late joiner or duplicate ACK
  c.acked = true;
  c.ok = a.ok;
  c.failed = !a.ok;
  c.rxCoreUs = a.rxCoreUs;
  c.capCoreUs = a.capCoreUs;
  c.bytes = a.bytes;
  c.capMs = a.capMs;
  c.ackAtMs = nowMs;
  uint32_t eff = kStartTransfer;  // transfers start as soon as the first frame is ready
  if (state_ == State::Shooting && s_.allAcked()) {
    s_.capturedAtMs = nowMs;
    eff |= kShotCaptured | enter(State::Transferring, nowMs);
  }
  return eff | finishIfSettled(nowMs);
}

uint32_t Controller::onStored(uint8_t cam, bool ok, uint32_t nowMs) {
  if (cam < 1 || cam > proto::kMaxCams) return kNone;
  CamShot& c = s_.cam[cam - 1];
  if (!c.expected) return kNone;
  c.inFlight = false;
  if (ok) {
    c.stored = true;
    c.storedAtMs = nowMs;
  } else {
    c.failed = true;
  }
  return kStartTransfer | finishIfSettled(nowMs);
}

uint32_t Controller::onTransferRetry(uint8_t cam) {
  if (cam < 1 || cam > proto::kMaxCams) return kNone;
  CamShot& c = s_.cam[cam - 1];
  c.inFlight = false;
  if (++c.tries > p_.retries) {
    c.failed = true;
    return kStartTransfer | finishIfSettled(0);
  }
  return kStartTransfer;
}

uint32_t Controller::finishIfSettled(uint32_t nowMs) {
  if (state_ != State::Shooting && state_ != State::Transferring) return kNone;
  if (!s_.allAcked() || !s_.settled()) return kNone;
  if (nowMs == 0) nowMs = s_.t0Ms + 1;
  s_.doneAtMs = nowMs;
  uint32_t eff = kShotStored;
  if (s_.failed() > 0) eff |= kShotPartial;
  if (s_.totalMs() > p_.budgetMs) eff |= kOverBudget;
  if (state_ == State::Shooting) {
    s_.capturedAtMs = nowMs;
    eff |= kShotCaptured;
  }
  state_ = State::Armed;  // temporary; reevaluate picks Armed/Preview/Idle/Error
  eff |= kStateChanged;
  uint32_t r = reevaluate(nowMs);
  if (state_ == State::Preview) eff |= kResumePreview;
  return eff | r;
}

uint32_t Controller::tick(uint32_t nowMs) {
  if (state_ != State::Shooting) return kNone;
  if ((uint32_t)(nowMs - s_.t0Ms) < p_.ackTimeoutMs + p_.leadMs) return kNone;
  // ACK timeout: whoever did not answer has failed this shot.
  for (auto& c : s_.cam)
    if (c.expected && !c.acked && !c.failed) c.failed = true;
  s_.capturedAtMs = nowMs;
  uint32_t eff = kShotCaptured | kStartTransfer | enter(State::Transferring, nowMs);
  return eff | finishIfSettled(nowMs);
}

uint8_t Controller::nextToFetch() {
  if (state_ != State::Shooting && state_ != State::Transferring) return 0;
  int inFlight = 0;
  for (const auto& c : s_.cam) inFlight += c.inFlight ? 1 : 0;
  if (inFlight >= p_.parallel) return 0;
  for (int i = 0; i < proto::kMaxCams; i++) {
    CamShot& c = s_.cam[i];
    if (c.expected && c.ok && !c.stored && !c.failed && !c.inFlight) {
      c.inFlight = true;
      return (uint8_t)(i + 1);
    }
  }
  return 0;
}

}  // namespace shoot
}  // namespace x4
