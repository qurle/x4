// Step 3: microSD. Write 1 MB, read it back twice, compare, report speed.
// Wiring: Core on USB, a FAT32 microSD in the slot (<= 16 GB: the CoreS3 maximum per M5 docs).
// Pass: "verify ok" at 25 MHz on screen; /x4cam_step3.bin exists on the card.
//
// The test runs at 25 MHz (what the firmware uses, and M5's own CoreS3 examples), then at 10 and
// 4 MHz. If only the slower clocks pass, the card or its contacts can't keep up with 25 MHz.
// Each run reads the file twice: two reads that disagree with each other mean the read path is
// unstable; two reads that agree but are wrong mean the data went onto the card wrong.
#include <SD.h>
#include <SPI.h>

#include "core_common.h"

constexpr size_t kChunk = 4096;
constexpr size_t kTotal = 1024 * 1024;
constexpr const char* kPath = "/x4cam_step3.bin";
constexpr uint32_t kClocks[] = {25000000, 10000000, 4000000};

uint8_t pattern(size_t i) { return (uint8_t)(i * 31 + (i >> 8)); }

static uint8_t buf[kChunk];

struct ReadResult {
  size_t got = 0;             // bytes read
  size_t bad = 0;             // bytes that differ from the pattern
  long firstBad = -1;         // offset of the first one
  uint8_t want = 0, saw = 0;  // its expected and actual value
  uint32_t ms = 0;
};

ReadResult readBack() {
  ReadResult r;
  File f = SD.open(kPath, FILE_READ);
  if (!f) return r;
  uint32_t t0 = millis();
  for (size_t off = 0; off < kTotal; off += kChunk) {
    size_t n = f.read(buf, kChunk);
    for (size_t i = 0; i < n; i++) {
      if (buf[i] == pattern(off + i)) continue;
      if (r.firstBad < 0) r.firstBad = off + i, r.want = pattern(off + i), r.saw = buf[i];
      r.bad++;
    }
    r.got += n;
    if (n < kChunk) break;
  }
  r.ms = millis() - t0;
  f.close();
  return r;
}

// One full write + two reads at the given SPI clock. Returns true if everything matched.
bool bench(int row, uint32_t hz) {
  unsigned mhz = hz / 1000000;
  SD.end();
  if (!SD.begin(pins::kSdCs, SPI, hz)) {
    Serial.printf("sd mount FAILED mhz=%u\n", mhz);
    screenLine(row, TFT_RED, "%u MHz: mount FAILED", mhz);
    return false;
  }
  Serial.printf("sd mount mhz=%u type=%d size_mb=%llu\n", mhz, SD.cardType(), SD.cardSize() >> 20);

  File f = SD.open(kPath, FILE_WRITE);
  if (!f) {
    Serial.printf("sd open FAILED mhz=%u\n", mhz);
    screenLine(row, TFT_RED, "%u MHz: can't create file", mhz);
    return false;
  }
  size_t written = 0;
  uint32_t t0 = millis();
  for (size_t off = 0; off < kTotal; off += kChunk) {
    for (size_t i = 0; i < kChunk; i++) buf[i] = pattern(off + i);
    written += f.write(buf, kChunk);
  }
  f.close();
  uint32_t writeMs = millis() - t0;

  f = SD.open(kPath, FILE_READ);
  size_t fileSize = f ? f.size() : 0;
  f.close();

  ReadResult r1 = readBack(), r2 = readBack();
  bool ok = written == kTotal && fileSize == kTotal && r1.got == kTotal && r1.bad == 0 &&
            r2.got == kTotal && r2.bad == 0;
  unsigned wk = writeMs ? kTotal / writeMs : 0;  // bytes per ms = kB/s
  unsigned rk = r1.ms ? kTotal / r1.ms : 0;

  Serial.printf("sd bench mhz=%u bytes=%u written=%u file_size=%u write_ms=%lu read_ms=%lu "
                "write_kbps=%u read_kbps=%u verify=%d\n",
                mhz, (unsigned)kTotal, (unsigned)written, (unsigned)fileSize,
                (unsigned long)writeMs, (unsigned long)r1.ms, wk, rk, ok);
  for (const ReadResult* r : {&r1, &r2}) {
    if (r->got == kTotal && r->bad == 0) continue;
    Serial.printf("sd verify mhz=%u read=%d got=%u bad=%u first_bad=%ld want=0x%02x saw=0x%02x\n",
                  mhz, r == &r1 ? 1 : 2, (unsigned)r->got, (unsigned)r->bad, r->firstBad,
                  r->want, r->saw);
  }

  if (ok) {
    screenLine(row, TFT_GREEN, "%u MHz ok: w %u, r %u kB/s", mhz, wk, rk);
  } else if (written != kTotal || fileSize != kTotal) {
    screenLine(row, TFT_RED, "%u MHz: write short (%u kB)", mhz, (unsigned)(fileSize >> 10));
  } else {
    screenLine(row, TFT_RED, "%u MHz: verify FAILED, bad %u/%u B", mhz, (unsigned)r1.bad,
               (unsigned)r2.bad);
  }
  return ok;
}

void setup() {
  beginCoreSafely();
  screenTitle("step 3: microSD");

  // The LCD and the SD card share one SPI bus. This sketch never draws while the card is busy.
  SPI.begin(pins::kSpiSck, pins::kSpiMiso, pins::kSpiMosi, pins::kSdCs);
  if (!SD.begin(pins::kSdCs, SPI, kClocks[0])) {
    Serial.println("sd mount FAILED");
    screenLine(0, TFT_RED, "mount FAILED: card in? FAT32?");
    return;
  }
  screenLine(0, TFT_WHITE, "card: %llu MB", SD.cardSize() >> 20);

  bool pass = false;
  for (size_t i = 0; i < sizeof(kClocks) / sizeof(kClocks[0]); i++) {
    bool ok = bench(1 + i, kClocks[i]);
    if (i == 0) pass = ok;
  }
  screenLine(5, pass ? TFT_GREEN : TFT_RED, pass ? "PASS" : "FAIL at 25 MHz: see serial log");
}

void loop() { delay(1000); }
