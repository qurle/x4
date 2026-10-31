// Optional post-processing carried over from earlier planning: ordered (Bayer) dithering of an
// 8-bit grayscale image to 1-bit or 16 levels, plus minimal BMP writers. Not used in the shooting
// path (a 5 MP decode does not fit the 5 s budget on the Core); kept for previews and as the
// reference implementation for the companion app. Pure, unit-tested.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace x4 {
namespace dither {

// 8x8 Bayer threshold for (x, y), 0..63.
uint8_t bayer8(uint32_t x, uint32_t y);

// gray: w*h bytes. out: packed 1-bit rows, MSB first, rows padded to bytes ((w+7)/8 per row).
void toMono(const uint8_t* gray, uint32_t w, uint32_t h, uint8_t* out);
// gray -> 16 levels (0..15) one byte per pixel (out may equal gray).
void to16(const uint8_t* gray, uint32_t w, uint32_t h, uint8_t* out);

// BMP sizes / headers (bottom-up rows, 4-byte row padding). Header buffers must be >= 62 / 118 B.
uint32_t bmpMonoRowBytes(uint32_t w);
uint32_t bmpMonoHeader(uint8_t* hdr, uint32_t w, uint32_t h);   // returns header size (62)
uint32_t bmp16RowBytes(uint32_t w);
uint32_t bmp16Header(uint8_t* hdr, uint32_t w, uint32_t h);     // 4-bit gray palette (118)

}  // namespace dither
}  // namespace x4
