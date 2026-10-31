#include "x4/safety.h"

namespace x4 {
namespace safety {

FlashGuard::FlashGuard(uint32_t pulseCapMs, uint32_t cooldownMs)
    : cap_(pulseCapMs > kFlashPulseAbsMaxMs ? kFlashPulseAbsMaxMs : pulseCapMs),
      cooldown_(cooldownMs < kFlashCooldownAbsMinMs ? kFlashCooldownAbsMinMs : cooldownMs) {
  if (cap_ == 0) cap_ = 1;
}

void FlashGuard::setLimits(uint32_t pulseCapMs, uint32_t cooldownMs) {
  cap_ = pulseCapMs > kFlashPulseAbsMaxMs ? kFlashPulseAbsMaxMs : (pulseCapMs ? pulseCapMs : 1);
  cooldown_ = cooldownMs < kFlashCooldownAbsMinMs ? kFlashCooldownAbsMinMs : cooldownMs;
}

uint32_t FlashGuard::readyInMs(uint32_t nowMs) const {
  if (!everFired_) return 0;
  // Cooldown counts from the end of the previous pulse (heat is generated while it is on).
  uint32_t since = nowMs - lastEnd_;
  return since >= cooldown_ ? 0 : cooldown_ - since;
}

FlashGuard::Verdict FlashGuard::request(uint32_t nowMs, uint32_t wantMs, uint32_t* grantedMs,
                                        uint32_t* waitMs) const {
  if (grantedMs) *grantedMs = 0;
  if (waitMs) *waitMs = 0;
  if (!enabled_) return Verdict::Disabled;
  if (tooHot_) return Verdict::TooHot;
  if (active_) return Verdict::Busy;
  uint32_t wait = readyInMs(nowMs);
  if (wait > 0) {
    if (waitMs) *waitMs = wait;
    return Verdict::Cooldown;
  }
  uint32_t g = wantMs == 0 ? cap_ : (wantMs > cap_ ? cap_ : wantMs);
  if (grantedMs) *grantedMs = g;
  return Verdict::Ok;
}

void FlashGuard::fired(uint32_t nowMs, uint32_t pulseMs) {
  active_ = true;
  everFired_ = true;
  lastStart_ = nowMs;
  granted_ = pulseMs > cap_ ? cap_ : pulseMs;
  lastEnd_ = nowMs + granted_;  // provisional, corrected by ended()
}

void FlashGuard::ended(uint32_t nowMs) {
  active_ = false;
  lastEnd_ = nowMs;
}

bool FlashGuard::overdue(uint32_t nowMs, uint32_t slackMs) const {
  if (!active_) return false;
  return (uint32_t)(nowMs - lastStart_) > granted_ + slackMs;
}

const char* FlashGuard::verdictName(Verdict v) {
  switch (v) {
    case Verdict::Ok: return "ok";
    case Verdict::Disabled: return "disabled";
    case Verdict::Cooldown: return "cooldown";
    case Verdict::TooHot: return "too_hot";
    case Verdict::Busy: return "busy";
  }
  return "?";
}

}  // namespace safety
}  // namespace x4
