#include "x4/errors.h"

namespace x4 {

const char* errName(Err e) {
  switch (e) {
    case Err::OK: return "OK";
#define X4_ERR(code, name, doc) \
  case Err::name: return #name;
#include "x4/errors.def"
#undef X4_ERR
  }
  return "UNKNOWN";
}

const char* errDoc(Err e) {
  switch (e) {
    case Err::OK: return "no error";
#define X4_ERR(code, name, doc) \
  case Err::name: return doc;
#include "x4/errors.def"
#undef X4_ERR
  }
  return "unknown error code";
}

}  // namespace x4
