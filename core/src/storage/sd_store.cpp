#include "sd_store.h"

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <esp_heap_caps.h>

#include "../hw/spi_lock.h"
#include "../log/log_setup.h"
#include "board_pins.h"
#include "x4/log.h"
#include "x4/storage_layout.h"

namespace x4core {
namespace sd {
namespace {

bool g_mounted = false;
Bench g_bench;
File g_logFile;
uint32_t g_logBytes = 0;

bool mkdirs() {
  SpiLock l;
  bool ok = true;
  if (!SD.exists(x4::storage::kRoot)) ok &= SD.mkdir(x4::storage::kRoot);
  if (!SD.exists(x4::storage::kLogsDir)) ok &= SD.mkdir(x4::storage::kLogsDir);
  return ok;
}

}  // namespace

bool begin() {
  for (int attempt = 0; attempt < 2 && !g_mounted; attempt++) {
    SpiLock l;
    // The display driver already owns these pins; SPI here only shares the bus (see spi_lock.h).
    SPI.begin(pins::kSpiSck, pins::kSpiMiso, pins::kSpiMosi, pins::kSdCs);
    g_mounted = SD.begin(pins::kSdCs, SPI, kSpiHz, "/sd", 5, false);
    if (!g_mounted) delay(50);
  }
  if (!g_mounted) {
    X4_LOG_ERR(Error, "sd", "mount", x4::Err::SD_MOUNT_FAIL, "cs=%d hz=%lu", pins::kSdCs,
               (unsigned long)kSpiHz);
    return false;
  }
  mkdirs();
  X4_LOGI("sd", "mount", "type=%s total_mb=%llu free_mb=%llu", cardTypeName(),
          (unsigned long long)(totalBytes() >> 20), (unsigned long long)(freeBytes() >> 20));
  return true;
}

bool mounted() { return g_mounted; }

uint64_t freeBytes() {
  if (!g_mounted) return 0;
  SpiLock l;
  return SD.totalBytes() - SD.usedBytes();
}

uint64_t totalBytes() {
  if (!g_mounted) return 0;
  SpiLock l;
  return SD.totalBytes();
}

const char* cardTypeName() {
  if (!g_mounted) return "none";
  switch (SD.cardType()) {
    case CARD_MMC: return "mmc";
    case CARD_SD: return "sdsc";
    case CARD_SDHC: return "sdhc";
    default: return "unknown";
  }
}

Bench benchmark(uint32_t kb) {
  Bench b;
  if (!g_mounted) return b;
  const char* path = "/x4/bench.bin";
  uint8_t* buf = (uint8_t*)heap_caps_malloc(kChunk, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!buf) return b;
  for (size_t i = 0; i < kChunk; i++) buf[i] = (uint8_t)(i * 31);
  uint32_t total = kb * 1024;
  uint32_t t0 = micros();
  File f;
  {
    SpiLock l;
    f = SD.open(path, FILE_WRITE);
  }
  bool ok = (bool)f;
  for (uint32_t w = 0; ok && w < total; w += kChunk) ok = writeChunk(f, buf, kChunk);
  closeFile(f);
  uint32_t t1 = micros();
  {
    SpiLock l;
    f = SD.open(path, FILE_READ);
  }
  uint32_t r = 0;
  while (ok && f && r < total) {
    SpiLock l;
    int n = f.read(buf, kChunk);
    if (n <= 0) break;
    r += (uint32_t)n;
  }
  closeFile(f);
  uint32_t t2 = micros();
  removeFile(path);
  heap_caps_free(buf);
  b.ok = ok && r == total;
  b.bytes = total;
  b.writeKBps = (t1 - t0) ? total / 1.024f / ((t1 - t0) / 1000.0f) : 0;
  b.readKBps = (t2 - t1) ? total / 1.024f / ((t2 - t1) / 1000.0f) : 0;
  g_bench = b;
  // ~1 MB/s write is the minimum for 4 x 1.5 MB inside the 5 s budget with WiFi in parallel.
  if (b.ok && b.writeKBps < 1000) {
    X4_LOG_ERR(Warn, "sd", "bench", x4::Err::SD_SLOW, "write_kbps=%.0f read_kbps=%.0f kb=%lu",
               b.writeKBps, b.readKBps, (unsigned long)kb);
  } else {
    X4_LOGI("sd", "bench", "ok=%d write_kbps=%.0f read_kbps=%.0f kb=%lu", b.ok, b.writeKBps,
            b.readKBps, (unsigned long)kb);
  }
  return b;
}

const Bench& lastBench() { return g_bench; }

File openCamFile(uint32_t shotId, uint8_t cam) {
  char path[48];
  x4::storage::camFile(path, sizeof(path), shotId, cam);
  SpiLock l;
  return SD.open(path, FILE_WRITE);
}

bool writeChunk(File& f, const uint8_t* data, size_t len) {
  SpiLock l;
  size_t w = f.write(data, len);
  if (w != len) {
    X4_LOG_ERR(Error, "sd", "write", x4::Err::SD_WRITE_FAIL, "want=%u wrote=%u", (unsigned)len,
               (unsigned)w);
    return false;
  }
  return true;
}

void closeFile(File& f) {
  SpiLock l;
  if (f) f.close();
}

bool removeFile(const char* path) {
  SpiLock l;
  return SD.remove(path);
}

void pumpLogs(uint32_t maxKb) {
  if (!g_mounted) return;
  char line[200];
  int n = 0;
  while (n < 16 && logsys::popSdLine(line, sizeof(line))) {
    SpiLock l;
    if (!g_logFile) {
      g_logFile = SD.open("/x4/logs/x4.log", FILE_APPEND);
      g_logBytes = g_logFile ? g_logFile.size() : 0;
    }
    if (!g_logFile) return;
    size_t len = strlen(line);
    g_logFile.write((const uint8_t*)line, len);
    g_logFile.write('\n');
    g_logBytes += len + 1;
    if (g_logBytes > maxKb * 1024) {
      g_logFile.close();
      SD.remove("/x4/logs/x4.1.log");
      SD.rename("/x4/logs/x4.log", "/x4/logs/x4.1.log");
      g_logBytes = 0;
    }
    n++;
  }
  if (n && g_logFile) {
    SpiLock l;
    g_logFile.flush();
  }
}

int listShots(ShotEntry* out, int max) {
  if (!g_mounted || max <= 0) return 0;
  File dir;
  {
    SpiLock l;
    dir = SD.open(x4::storage::kRoot);
  }
  if (!dir) return 0;
  int n = 0;  // out[0..n) sorted by id, newest first
  for (;;) {
    bool isDir = false;
    String name;
    {
      SpiLock l;  // one directory entry per lock, so a big folder doesn't stall the UI
      name = dir.getNextFileName(&isDir);
    }
    if (!name.length()) break;
    uint32_t id;
    uint8_t cam;
    if (isDir || !x4::storage::parseCamFile(name.c_str(), &id, &cam) || cam > 8) continue;
    int i = 0;
    while (i < n && out[i].id > id) i++;
    if (i < n && out[i].id == id) {
      out[i].cams |= (uint8_t)(1u << (cam - 1));
      continue;
    }
    if (i >= max) continue;  // older than everything kept
    if (n < max) n++;
    memmove(&out[i + 1], &out[i], (size_t)(n - 1 - i) * sizeof(ShotEntry));
    out[i] = ShotEntry{id, (uint8_t)(1u << (cam - 1))};
  }
  SpiLock l;
  dir.close();
  return n;
}

}  // namespace sd
}  // namespace x4core
