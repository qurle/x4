// Error codes used in logs (err=<code>), HTTP responses and on screen.
// The catalogue lives in errors.def so docs can be generated from it.
#pragma once
#include <stdint.h>

namespace x4 {

enum class Err : uint16_t {
  OK = 0,
#define X4_ERR(code, name, doc) name = code,
#include "errors.def"
#undef X4_ERR
};

// Stable snake-ish name, e.g. "SD_MOUNT_FAIL". Returns "UNKNOWN" for codes not in the table.
const char* errName(Err e);
// Human description from errors.def.
const char* errDoc(Err e);

}  // namespace x4
