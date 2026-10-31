// SD card layout (FAT32, readable on any PC). Pure path/sidecar builders so they are tested.
//
//   /x4/shots/S000042/cam1.jpg ... cam4.jpg
//   /x4/shots/S000042/shot.json        sidecar: settings, timings, per-camera results
//   /x4/logs/x4.log, x4.1.log          optional rotating logs (cfg log_sd)
//   /x4/bench.bin                      temporary SD speed-test file (deleted after the test)
//
// Folder names use the persistent shot counter, not the clock: the SE has an RTC but nothing sets
// it on-site. If the RTC is valid the sidecar records the wall-clock time too.
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "shoot.h"

namespace x4 {
namespace storage {

constexpr const char* kRoot = "/x4";
constexpr const char* kShotsDir = "/x4/shots";
constexpr const char* kLogsDir = "/x4/logs";

size_t shotDir(char* buf, size_t cap, uint32_t shotId);                 // /x4/shots/S000042
size_t camFile(char* buf, size_t cap, uint32_t shotId, uint8_t cam);    // .../cam1.jpg
size_t sidecarFile(char* buf, size_t cap, uint32_t shotId);             // .../shot.json

struct SidecarInfo {
  const shoot::Session* s;
  const char* fw;
  const char* git;
  const char* rtcIso;  // "" if RTC not valid
  int32_t res;
  int32_t quality;
  bool flash;
  int32_t tempC;
};
// Writes the JSON sidecar. Returns length, 0 on overflow.
size_t sidecarJson(char* buf, size_t cap, const SidecarInfo& info);

}  // namespace storage
}  // namespace x4
