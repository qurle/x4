// Step 3: microSD. Write 1 MB, read it back, compare, report speed.
// Wiring: Core on USB, a FAT32 microSD in the slot (<= 32 GB is the safe choice).
// Pass: "verify ok" and write/read speeds on screen; /x4cam_step3.bin exists on the card.
#include <SD.h>
#include <SPI.h>

#include "core_common.h"

constexpr size_t kChunk = 4096;
constexpr size_t kTotal = 1024 * 1024;
constexpr const char* kPath = "/x4cam_step3.bin";

uint8_t pattern(size_t i) { return (uint8_t)(i * 31 + (i >> 8)); }

void setup() {
  beginCoreSafely();
  screenTitle("step 3: microSD");

  // The LCD and the SD card share one SPI bus. This sketch never draws while the card is busy.
  SPI.begin(pins::kSpiSck, pins::kSpiMiso, pins::kSpiMosi, pins::kSdCs);
  if (!SD.begin(pins::kSdCs, SPI, 25000000)) {
    Serial.println("sd mount FAILED");
    screenLine(0, TFT_RED, "mount FAILED: card in? FAT32?");
    return;
  }
  Serial.printf("sd mount type=%d size_mb=%llu\n", SD.cardType(), SD.cardSize() >> 20);

  static uint8_t buf[kChunk];
  File f = SD.open(kPath, FILE_WRITE);
  uint32_t t0 = millis();
  for (size_t off = 0; off < kTotal; off += kChunk) {
    for (size_t i = 0; i < kChunk; i++) buf[i] = pattern(off + i);
    f.write(buf, kChunk);
  }
  f.close();
  uint32_t writeMs = millis() - t0;

  f = SD.open(kPath, FILE_READ);
  bool same = f && f.size() == kTotal;
  t0 = millis();
  for (size_t off = 0; same && off < kTotal; off += kChunk) {
    f.read(buf, kChunk);
    for (size_t i = 0; i < kChunk; i++) same &= buf[i] == pattern(off + i);
  }
  f.close();
  uint32_t readMs = millis() - t0;

  unsigned wk = writeMs ? kTotal / writeMs : 0;  // bytes per ms = kB/s
  unsigned rk = readMs ? kTotal / readMs : 0;
  Serial.printf("sd bench bytes=%u write_ms=%lu read_ms=%lu write_kbps=%u read_kbps=%u verify=%d\n",
                (unsigned)kTotal, (unsigned long)writeMs, (unsigned long)readMs, wk, rk, same);
  screenLine(0, TFT_WHITE, "card: %llu MB", SD.cardSize() >> 20);
  screenLine(1, TFT_WHITE, "write 1 MB: %lu ms (%u kB/s)", (unsigned long)writeMs, wk);
  screenLine(2, TFT_WHITE, "read 1 MB: %lu ms (%u kB/s)", (unsigned long)readMs, rk);
  screenLine(3, same ? TFT_GREEN : TFT_RED, same ? "verify ok" : "verify FAILED");
}

void loop() { delay(1000); }
