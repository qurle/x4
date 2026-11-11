// SD card layout (FAT32, readable on any PC). Pure path / metadata builders so they are tested.
//
//   /x4/000042-1.jpg ... 000042-4.jpg   one shot = 4 files, flat, no folders (owner, 2026-11-11)
//   /x4/logs/x4.log, x4.1.log           optional rotating logs (cfg log_sd)
//   /x4/bench.bin                       temporary SD speed-test file (deleted after the test)
//
// Names use the persistent shot counter only, never the clock: the SE's BM8563 has no backup
// battery (BAT1 not fitted, M5 schematic) and the power switch cuts the Core, so the RTC forgets
// the time at every power-off (ASSUMPTION, follows from the schematic). "000042-1.jpg" is a valid
// 8.3 name, so each photo takes one FAT directory entry.
//
// The shot settings travel inside each JPEG as a COM segment holding one JSON object
// (format "x4-shot/2"); there is no sidecar file. Timings go to the log ("shoot" "cam" lines).
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace x4 {
namespace storage {

constexpr const char* kRoot = "/x4";       // photos live directly in here
constexpr const char* kLogsDir = "/x4/logs";
constexpr const char* kShotFormat = "x4-shot/2";

size_t camFile(char* buf, size_t cap, uint32_t shotId, uint8_t cam);  // /x4/000042-3.jpg
size_t camFileName(char* buf, size_t cap, uint32_t shotId, uint8_t cam);  // 000042-3.jpg
// Parses a photo file name ("000042-3.jpg", any letter case; a leading path is ignored).
bool parseCamFile(const char* name, uint32_t* shotId, uint8_t* cam);

// Settings of one shot, snapshotted at the shutter so a menu change during transfer can't leak in.
struct ShotMeta {
  uint32_t shot;
  const char* fw;
  const char* git;
  int32_t res;
  int32_t quality;
  bool flash;
  int32_t tempC;
  const char* orientation;  // "horizontal" / "portrait"
  const char* effect;       // "normal" / "dithered" (the photo itself stays original)
  int32_t camDelayMs;
};

// Writes a complete JPEG COM segment (FF FE, 2-byte length, JSON text) for camera `cam`.
// Returns its length in bytes, 0 if it does not fit in `cap`.
size_t jpegComment(uint8_t* buf, size_t cap, const ShotMeta& m, uint8_t cam);

// Where to insert the COM segment in a JPEG that starts at p[0]: right after SOI and any APPn
// segments (JFIF / EXIF readers expect those first). Returns the offset (>= 2), 0 if more bytes
// are needed to decide, -1 if p does not start with SOI (FF D8).
long commentInsertAt(const uint8_t* p, size_t n);

// Streams a JPEG through and inserts a COM segment after its header (SOI + APPn), as the bytes
// arrive. Up to `holdCap` header bytes are held back; a longer header gets the segment right after
// SOI instead, and a stream that is not a JPEG is passed through unchanged.
class CommentInserter {
 public:
  using Write = bool (*)(void* ctx, const uint8_t* p, size_t n);
  CommentInserter(uint8_t* hold, size_t holdCap, const uint8_t* com, size_t comLen, Write write, void* ctx)
      : hold_(hold), cap_(holdCap), com_(com), comLen_(comLen), write_(write), ctx_(ctx), done_(comLen == 0) {}
  bool feed(const uint8_t* p, size_t n);  // false if a write failed
  bool finish();                          // flushes a header still held (stream shorter than it)
  bool inserted() const { return inserted_; }

 private:
  bool flush(long at);
  uint8_t* hold_;
  size_t cap_;
  const uint8_t* com_;
  size_t comLen_;
  Write write_;
  void* ctx_;
  size_t held_ = 0;
  bool done_;
  bool inserted_ = false;
};

// Finds the x4 COM segment in a JPEG (scans header segments up to SOS). On success points
// *text at the JSON (not NUL-terminated) and returns its length; 0 if there is none.
size_t findComment(const uint8_t* p, size_t n, const char** text);

}  // namespace storage
}  // namespace x4
