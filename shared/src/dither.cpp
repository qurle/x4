#include "x4/dither.h"

#include <string.h>

namespace x4 {
namespace dither {
namespace {

const uint8_t kBayer8[8][8] = {
    {0, 32, 8, 40, 2, 34, 10, 42},  {48, 16, 56, 24, 50, 18, 58, 26},
    {12, 44, 4, 36, 14, 46, 6, 38}, {60, 28, 52, 20, 62, 30, 54, 22},
    {3, 35, 11, 43, 1, 33, 9, 41},  {51, 19, 59, 27, 49, 17, 57, 25},
    {15, 47, 7, 39, 13, 45, 5, 37}, {63, 31, 55, 23, 61, 29, 53, 21}};

void le16(uint8_t* p, uint16_t v) {
  p[0] = v & 0xFF;
  p[1] = v >> 8;
}
void le32(uint8_t* p, uint32_t v) {
  for (int i = 0; i < 4; i++) p[i] = (v >> (8 * i)) & 0xFF;
}

uint32_t header(uint8_t* hdr, uint32_t w, uint32_t h, uint16_t bpp, uint32_t colors,
                uint32_t rowBytes) {
  const uint32_t palBytes = colors * 4;
  const uint32_t off = 14 + 40 + palBytes;
  const uint32_t img = rowBytes * h;
  memset(hdr, 0, off);
  hdr[0] = 'B';
  hdr[1] = 'M';
  le32(hdr + 2, off + img);
  le32(hdr + 10, off);
  le32(hdr + 14, 40);
  le32(hdr + 18, w);
  le32(hdr + 22, h);
  le16(hdr + 26, 1);
  le16(hdr + 28, bpp);
  le32(hdr + 34, img);
  le32(hdr + 38, 2835);  // 72 dpi
  le32(hdr + 42, 2835);
  le32(hdr + 46, colors);
  for (uint32_t i = 0; i < colors; i++) {
    uint8_t g = (uint8_t)(colors == 2 ? (i ? 255 : 0) : i * 17);
    hdr[54 + i * 4 + 0] = g;
    hdr[54 + i * 4 + 1] = g;
    hdr[54 + i * 4 + 2] = g;
  }
  return off;
}

}  // namespace

uint8_t bayer8(uint32_t x, uint32_t y) { return kBayer8[y & 7][x & 7]; }

void toMono(const uint8_t* gray, uint32_t w, uint32_t h, uint8_t* out) {
  const uint32_t row = (w + 7) / 8;
  memset(out, 0, (size_t)row * h);
  for (uint32_t y = 0; y < h; y++) {
    for (uint32_t x = 0; x < w; x++) {
      // threshold in 0..255: (b + 0.5) * 4
      uint32_t t = bayer8(x, y) * 4u + 2u;
      if (gray[(size_t)y * w + x] > t) out[(size_t)y * row + x / 8] |= (uint8_t)(0x80 >> (x & 7));
    }
  }
}

void to16(const uint8_t* gray, uint32_t w, uint32_t h, uint8_t* out) {
  for (uint32_t y = 0; y < h; y++) {
    for (uint32_t x = 0; x < w; x++) {
      uint32_t v = gray[(size_t)y * w + x];
      // 15 steps of 17; add a Bayer offset in [0,17) before quantising.
      uint32_t q = (v * 15u * 64u + bayer8(x, y) * 255u) / (255u * 64u);
      if (q > 15) q = 15;
      out[(size_t)y * w + x] = (uint8_t)q;
    }
  }
}

uint32_t bmpMonoRowBytes(uint32_t w) { return ((w + 31) / 32) * 4; }
uint32_t bmp16RowBytes(uint32_t w) { return ((w * 4 + 31) / 32) * 4; }
uint32_t bmpMonoHeader(uint8_t* hdr, uint32_t w, uint32_t h) {
  return header(hdr, w, h, 1, 2, bmpMonoRowBytes(w));
}
uint32_t bmp16Header(uint8_t* hdr, uint32_t w, uint32_t h) {
  return header(hdr, w, h, 4, 16, bmp16RowBytes(w));
}

}  // namespace dither
}  // namespace x4
