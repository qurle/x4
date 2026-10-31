#include "x4/storage_layout.h"

#include <inttypes.h>
#include <stdio.h>

namespace x4 {
namespace storage {
namespace {
size_t fin(int n, size_t cap) { return (n < 0 || (size_t)n >= cap) ? 0 : (size_t)n; }
}  // namespace

size_t shotDir(char* buf, size_t cap, uint32_t shotId) {
  return fin(snprintf(buf, cap, "%s/S%06" PRIu32, kShotsDir, shotId), cap);
}

size_t camFile(char* buf, size_t cap, uint32_t shotId, uint8_t cam) {
  return fin(snprintf(buf, cap, "%s/S%06" PRIu32 "/cam%u.jpg", kShotsDir, shotId, (unsigned)cam),
             cap);
}

size_t sidecarFile(char* buf, size_t cap, uint32_t shotId) {
  return fin(snprintf(buf, cap, "%s/S%06" PRIu32 "/shot.json", kShotsDir, shotId), cap);
}

size_t sidecarJson(char* buf, size_t cap, const SidecarInfo& in) {
  if (!in.s || !buf) return 0;
  const shoot::Session& s = *in.s;
  size_t w = 0;
  auto put = [&](int n) {
    if (n < 0 || w + (size_t)n >= cap) {
      w = cap;  // overflow marker
      return;
    }
    w += (size_t)n;
  };
  put(snprintf(buf + w, cap - w,
               "{\"format\":\"x4-shot/1\",\"shot\":%" PRIu32 ",\"fw\":\"%s\",\"git\":\"%s\","
               "\"rtc\":\"%s\",\"res\":%ld,\"quality\":%ld,\"flash\":%s,\"temp_c\":%ld,"
               "\"t0_ms\":%" PRIu32 ",\"at_us\":%" PRId64 ",\"captured_ms\":%" PRIu32
               ",\"done_ms\":%" PRIu32 ",\"total_ms\":%" PRIu32 ",\"skew_us\":%" PRId64
               ",\"cams\":[",
               s.id, in.fw ? in.fw : "", in.git ? in.git : "", in.rtcIso ? in.rtcIso : "",
               (long)in.res, (long)in.quality, in.flash ? "true" : "false", (long)in.tempC,
               s.t0Ms, s.atUs, s.capturedAtMs, s.doneAtMs, s.totalMs(), s.skewUs()));
  bool first = true;
  for (int i = 0; i < proto::kMaxCams && w < cap; i++) {
    const shoot::CamShot& c = s.cam[i];
    if (!c.expected) continue;
    put(snprintf(buf + w, cap - w,
                 "%s{\"cam\":%d,\"ok\":%s,\"stored\":%s,\"bytes\":%" PRIu32 ",\"cap_ms\":%" PRIu32
                 ",\"cap_us\":%" PRId64 ",\"rx_us\":%" PRId64 ",\"stored_ms\":%" PRIu32
                 ",\"tries\":%u,\"file\":\"cam%d.jpg\"}",
                 first ? "" : ",", i + 1, c.ok ? "true" : "false", c.stored ? "true" : "false",
                 c.bytes, c.capMs, c.capCoreUs, c.rxCoreUs, c.storedAtMs, (unsigned)c.tries,
                 i + 1));
    first = false;
  }
  if (w < cap) put(snprintf(buf + w, cap - w, "]}\n"));
  return w >= cap ? 0 : w;
}

}  // namespace storage
}  // namespace x4
