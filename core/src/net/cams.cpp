#include "cams.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string.h>

#include "x4/log.h"

namespace x4core {
namespace cams {
namespace {

SemaphoreHandle_t g_mu = nullptr;
Cam g_cams[x4::proto::kMaxCams];

struct Lock {
  Lock() { xSemaphoreTake(g_mu, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(g_mu); }
};

void ipStr(uint32_t ip, char* out, size_t cap) {
  IPAddress a(ip);
  snprintf(out, cap, "%u.%u.%u.%u", a[0], a[1], a[2], a[3]);
}

}  // namespace

int Snapshot::online() const {
  int n = 0;
  for (const auto& c : cam) n += c.online ? 1 : 0;
  return n;
}

void begin() { g_mu = xSemaphoreCreateMutex(); }

bool registerCam(uint8_t id, uint32_t ip, const char* fw, const char* git, const char* mac,
                 uint16_t pid, uint8_t hwRev, uint32_t nowMs) {
  if (id < 1 || id > x4::proto::kMaxCams) return false;
  bool wasOnline;
  {
    Lock l;
    Cam& c = g_cams[id - 1];
    wasOnline = c.online;
    c.registered = true;
    c.online = true;
    c.ip = ip;
    strncpy(c.fw, fw ? fw : "", sizeof(c.fw) - 1);
    strncpy(c.git, git ? git : "", sizeof(c.git) - 1);
    strncpy(c.mac, mac ? mac : "", sizeof(c.mac) - 1);
    c.sensorPid = pid;
    c.hwRev = hwRev;
    c.regAtMs = nowMs;
    c.lastHbMs = nowMs;
  }
  char ips[16];
  ipStr(ip, ips, sizeof(ips));
  X4_LOGI("net", "register", "cam=%u ip=%s fw=%s git=%s mac=%s sensor=0x%04x hw_rev=%u was_online=%d", id,
          ips, fw, git, mac, pid, hwRev, wasOnline);
  return true;
}

void onHeartbeat(const x4::proto::Heartbeat& hb, uint32_t ip, uint32_t nowMs) {
  if (hb.cam < 1 || hb.cam > x4::proto::kMaxCams) return;
  bool cameOnline = false;
  {
    Lock l;
    Cam& c = g_cams[hb.cam - 1];
    cameOnline = !c.online;
    c.online = true;
    c.ip = ip;
    c.hb = hb;
    c.lastHbMs = nowMs;
    c.hbCount++;
  }
  if (cameOnline) {
    char ips[16];
    ipStr(ip, ips, sizeof(ips));
    X4_LOGI("net", "cam_online", "cam=%u ip=%s via=heartbeat", hb.cam, ips);
  }
}

bool sweep(uint32_t nowMs, uint32_t timeoutMs) {
  bool changed = false;
  for (int i = 0; i < x4::proto::kMaxCams; i++) {
    bool dropped = false;
    uint32_t age = 0;
    {
      Lock l;
      Cam& c = g_cams[i];
      age = nowMs - c.lastHbMs;
      if (c.online && age > timeoutMs) {
        c.online = false;
        c.offlineEvents++;
        dropped = true;
      }
    }
    if (dropped) {
      changed = true;
      X4_LOG_ERR(Warn, "net", "cam_offline", x4::Err::NET_CAM_OFFLINE, "cam=%d last_hb_ms=%lu", i + 1,
                 (unsigned long)age);
    }
  }
  return changed;
}

Snapshot snapshot() {
  Snapshot s;
  Lock l;
  memcpy(s.cam, g_cams, sizeof(g_cams));
  return s;
}

uint32_t ipOf(uint8_t id) {
  if (id < 1 || id > x4::proto::kMaxCams) return 0;
  Lock l;
  return g_cams[id - 1].ip;
}

void onlineMask(bool out[x4::proto::kMaxCams]) {
  Lock l;
  for (int i = 0; i < x4::proto::kMaxCams; i++) out[i] = g_cams[i].online;
}

}  // namespace cams
}  // namespace x4core
