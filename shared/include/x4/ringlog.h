// Fixed-size in-RAM ring of recent log lines. Backs the diagnostics screen and GET /logs.
// No heap allocation; one mutex; O(1) append. Lines longer than Width-1 are cut.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <mutex>

namespace x4 {

template <size_t Lines, size_t Width>
class RingLog {
 public:
  // Appends a line. seq numbers start at 1 and never repeat (until 2^32).
  uint32_t append(const char* line, size_t len) {
    std::lock_guard<std::mutex> g(mu_);
    if (len >= Width) len = Width - 1;
    Slot& s = slots_[head_];
    memcpy(s.text, line, len);
    s.text[len] = 0;
    s.seq = ++seq_;
    head_ = (head_ + 1) % Lines;
    if (count_ < Lines) count_++;
    return s.seq;
  }

  // Calls fn(seq, text) for every stored line with seq > since, oldest first.
  // Returns the newest seq seen (pass it as `since` next time). fn runs under the lock: keep it short.
  template <typename Fn>
  uint32_t forEachSince(uint32_t since, Fn fn) const {
    std::lock_guard<std::mutex> g(mu_);
    size_t start = (head_ + Lines - count_) % Lines;
    uint32_t last = since;
    for (size_t i = 0; i < count_; i++) {
      const Slot& s = slots_[(start + i) % Lines];
      if (s.seq > since) {
        fn(s.seq, s.text);
        last = s.seq;
      }
    }
    return last;
  }

  // Copies the newest `maxLines` lines into out (newline-separated). Returns bytes written.
  size_t tail(char* out, size_t cap, size_t maxLines) const {
    std::lock_guard<std::mutex> g(mu_);
    if (cap == 0) return 0;
    size_t n = maxLines < count_ ? maxLines : count_;
    size_t start = (head_ + Lines - n) % Lines;
    size_t w = 0;
    for (size_t i = 0; i < n; i++) {
      const Slot& s = slots_[(start + i) % Lines];
      size_t l = strlen(s.text);
      if (w + l + 2 > cap) break;
      memcpy(out + w, s.text, l);
      w += l;
      out[w++] = '\n';
    }
    out[w] = 0;
    return w;
  }

  size_t size() const {
    std::lock_guard<std::mutex> g(mu_);
    return count_;
  }
  uint32_t lastSeq() const {
    std::lock_guard<std::mutex> g(mu_);
    return seq_;
  }

 private:
  struct Slot {
    uint32_t seq = 0;
    char text[Width] = {0};
  };
  mutable std::mutex mu_;
  Slot slots_[Lines];
  size_t head_ = 0;
  size_t count_ = 0;
  uint32_t seq_ = 0;
};

}  // namespace x4
