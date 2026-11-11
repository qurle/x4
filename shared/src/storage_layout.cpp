#include "x4/storage_layout.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

namespace x4 {
namespace storage {
namespace {
size_t fin(int n, size_t cap) { return (n < 0 || (size_t)n >= cap) ? 0 : (size_t)n; }
bool isDigit(char c) { return c >= '0' && c <= '9'; }
char lower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c; }
size_t segLen(const uint8_t* p) { return ((size_t)p[0] << 8) | p[1]; }
}  // namespace

size_t camFile(char* buf, size_t cap, uint32_t shotId, uint8_t cam) {
  return fin(snprintf(buf, cap, "%s/%06" PRIu32 "-%u.jpg", kRoot, shotId, (unsigned)cam), cap);
}

size_t camFileName(char* buf, size_t cap, uint32_t shotId, uint8_t cam) {
  return fin(snprintf(buf, cap, "%06" PRIu32 "-%u.jpg", shotId, (unsigned)cam), cap);
}

bool parseCamFile(const char* name, uint32_t* shotId, uint8_t* cam) {
  if (!name) return false;
  const char* slash = strrchr(name, '/');
  if (slash) name = slash + 1;
  // Exactly "NNNNNN-C.jpg": 6 digits, '-', camera 1-9, ".jpg".
  if (strlen(name) != 12) return false;
  for (int i = 0; i < 6; i++)
    if (!isDigit(name[i])) return false;
  if (name[6] != '-' || name[7] < '1' || name[7] > '9') return false;
  if (name[8] != '.' || lower(name[9]) != 'j' || lower(name[10]) != 'p' || lower(name[11]) != 'g') return false;
  uint32_t id = 0;
  for (int i = 0; i < 6; i++) id = id * 10 + (uint32_t)(name[i] - '0');
  if (shotId) *shotId = id;
  if (cam) *cam = (uint8_t)(name[7] - '0');
  return true;
}

size_t jpegComment(uint8_t* buf, size_t cap, const ShotMeta& m, uint8_t cam) {
  if (!buf || cap < 5) return 0;
  char* text = (char*)buf + 4;
  size_t room = cap - 4;
  int n = snprintf(text, room,
                   "{\"format\":\"%s\",\"shot\":%" PRIu32 ",\"cam\":%u,\"orientation\":\"%s\","
                   "\"effect\":\"%s\",\"res\":%ld,\"quality\":%ld,\"flash\":%s,\"cam_delay_ms\":%ld,"
                   "\"temp_c\":%ld,\"fw\":\"%s\",\"git\":\"%s\"}",
                   kShotFormat, m.shot, (unsigned)cam, m.orientation ? m.orientation : "horizontal",
                   m.effect ? m.effect : "normal", (long)m.res, (long)m.quality, m.flash ? "true" : "false",
                   (long)m.camDelayMs, (long)m.tempC, m.fw ? m.fw : "", m.git ? m.git : "");
  // snprintf needs room for its NUL; the segment itself carries no NUL. Max segment is 64 KB.
  if (n < 0 || (size_t)n >= room || n > 65533) return 0;
  size_t len = (size_t)n + 2;  // the length field counts itself
  buf[0] = 0xFF;
  buf[1] = 0xFE;
  buf[2] = (uint8_t)(len >> 8);
  buf[3] = (uint8_t)(len & 0xFF);
  return (size_t)n + 4;
}

long commentInsertAt(const uint8_t* p, size_t n) {
  if (!p || n < 2) return 0;
  if (p[0] != 0xFF || p[1] != 0xD8) return -1;
  size_t i = 2;
  for (;;) {
    if (i + 2 > n) return 0;
    if (p[i] != 0xFF) return (long)i;  // not a marker: don't guess, insert here
    uint8_t m = p[i + 1];
    if (m < 0xE0 || m > 0xEF) return (long)i;  // first non-APPn segment
    if (i + 4 > n) return 0;
    size_t len = segLen(p + i + 2);
    if (len < 2) return (long)i;  // broken length: stop here
    i += 2 + len;
  }
}

bool CommentInserter::flush(long at) {
  done_ = true;
  size_t n = held_;
  held_ = 0;
  if (at < 0 || n < 2) return write_(ctx_, hold_, n);  // not a JPEG (or 1 byte): pass through
  size_t cut = at > 0 ? (size_t)at : 2;
  if (cut > n) cut = n;
  inserted_ = true;
  return write_(ctx_, hold_, cut) && write_(ctx_, com_, comLen_) && (cut == n || write_(ctx_, hold_ + cut, n - cut));
}

bool CommentInserter::feed(const uint8_t* p, size_t n) {
  while (!done_ && n) {
    size_t take = cap_ - held_ < n ? cap_ - held_ : n;
    memcpy(hold_ + held_, p, take);
    held_ += take;
    p += take;
    n -= take;
    long at = commentInsertAt(hold_, held_);
    if (at != 0 || held_ == cap_) {
      if (!flush(at)) return false;
    }
  }
  return n == 0 || write_(ctx_, p, n);
}

bool CommentInserter::finish() {
  if (done_ || !held_) return true;
  // The stream ended inside its header (a broken photo; the transfer reports it): keep the bytes.
  return flush(commentInsertAt(hold_, held_));
}

size_t findComment(const uint8_t* p, size_t n, const char** text) {
  if (!p || n < 4 || p[0] != 0xFF || p[1] != 0xD8) return 0;
  static const char kTag[] = "{\"format\":\"x4-shot/";
  size_t i = 2;
  while (i + 4 <= n && p[i] == 0xFF) {
    uint8_t m = p[i + 1];
    if (m == 0xDA || m == 0xD9) break;  // SOS / EOI: header is over
    size_t len = segLen(p + i + 2);
    if (len < 2 || i + 2 + len > n) break;
    if (m == 0xFE && len - 2 >= sizeof(kTag) - 1 && memcmp(p + i + 4, kTag, sizeof(kTag) - 1) == 0) {
      if (text) *text = (const char*)(p + i + 4);
      return len - 2;
    }
    i += 2 + len;
  }
  return 0;
}

}  // namespace storage
}  // namespace x4
