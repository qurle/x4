// microSD on the Core: mount, free-space checks, write-speed benchmark, shot files, SD log file.
// The card shares the LCD SPI bus (VERIFIED, M5 docs), so every access goes through SpiLock and
// big writes are chunked (<= 16 KB per lock) to let LVGL flush in between.
// M5 docs: "microSD max 16 GB" for the CoreS3 family; the owner's card is 16 GB SDHC (OK).
#pragma once
#include <FS.h>
#include <stddef.h>
#include <stdint.h>

namespace x4core {
namespace sd {

constexpr uint32_t kSpiHz = 25000000;  // 25 MHz: M5's own CoreS3 examples use this value
constexpr size_t kChunk = 16 * 1024;

bool begin();          // mount (retries once); creates /x4, /x4/logs
bool mounted();
uint64_t freeBytes();
uint64_t totalBytes();
const char* cardTypeName();

struct Bench {
  bool ok = false;
  uint32_t bytes = 0;
  float writeKBps = 0;
  float readKBps = 0;
};
// Writes then reads a temporary file of `kb` KB in kChunk pieces. ~1-3 s for 1024 KB.
Bench benchmark(uint32_t kb);
const Bench& lastBench();

// Shot file helpers (paths from shared/x4/storage_layout.h).
File openCamFile(uint32_t shotId, uint8_t cam);           // "w"
bool writeChunk(File& f, const uint8_t* data, size_t len);  // locks the bus per call
void closeFile(File& f);
bool removeFile(const char* path);

// SD log sink: drains the log queue into /x4/logs/x4.log (rotates at log_sd_max_kb).
void pumpLogs(uint32_t maxKb);

// The newest shots found in /x4 (files "000042-1.jpg" ...), newest first. Returns count.
// One pass over the folder by name only (no per-file open), so it stays linear in the file count.
struct ShotEntry {
  uint32_t id;
  uint8_t cams;  // bit (cam-1) set if that camera's photo is on the card
};
int listShots(ShotEntry* out, int max);

}  // namespace sd
}  // namespace x4core
