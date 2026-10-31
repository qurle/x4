// UDP: one receive task (heartbeats -> immediate clock-sync answer, ACKs -> app queue, camera log
// lines -> log sinks) and one high-priority sender task for the SHOOT fan-out.
#include <Arduino.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <lwip/sockets.h>

#include "../log/log_setup.h"
#include "cams.h"
#include "net.h"
#include "x4/log.h"

namespace x4core {
namespace net {
namespace {

using namespace x4::proto;

int g_sock = -1;
QueueHandle_t g_ackQ = nullptr;
QueueHandle_t g_shootQ = nullptr;

struct ShootJob {
  ShootCmd cmd;
  uint8_t repeat;
};

void sendTo(uint32_t ip, const char* buf, size_t len) {
  sockaddr_in to = {};
  to.sin_family = AF_INET;
  to.sin_port = htons(kUdpCamPort);
  to.sin_addr.s_addr = ip;
  sendto(g_sock, buf, len, 0, (sockaddr*)&to, sizeof(to));
}

void rxTask(void*) {
  char buf[256];
  for (;;) {
    sockaddr_in from = {};
    socklen_t fl = sizeof(from);
    int n = recvfrom(g_sock, buf, sizeof(buf) - 1, 0, (sockaddr*)&from, &fl);
    if (n <= 0) continue;
    int64_t t2 = esp_timer_get_time();  // stamp first: clock-sync accuracy depends on it
    buf[n] = 0;
    KvLine kv;
    if (!kv.parse(buf, (size_t)n)) {
      X4_LOG_EVERY(5000, x4::log::Level::Warn, "net", "udp_bad", "len=%d", n);
      continue;
    }
    if (kv.version() != X4_PROTOCOL_VERSION) {
      X4_LOG_ERR(Warn, "net", "udp_version", x4::Err::NET_PROTO_MISMATCH, "got=%d want=%d", kv.version(),
                 X4_PROTOCOL_VERSION);
      continue;
    }
    switch (kv.type()) {
      case MsgType::Heartbeat: {
        Heartbeat hb;
        if (!parseHeartbeat(kv, &hb)) break;
        HbAck a{hb.seq, hb.t1Us, t2};
        char out[96];
        size_t len = buildHbAck(out, sizeof(out), a);
        sendto(g_sock, out, len, 0, (sockaddr*)&from, fl);  // answer to the sender's port
        cams::onHeartbeat(hb, from.sin_addr.s_addr, millis());
        X4_LOGT("net", "hb", "cam=%u seq=%lu temp=%d heap=%lu rssi=%d", hb.cam, (unsigned long)hb.seq,
                hb.tempC, (unsigned long)hb.heap, hb.rssi);
        break;
      }
      case MsgType::Ack: {
        ShotAck a;
        if (parseShotAck(kv, &a)) xQueueSend(g_ackQ, &a, 0);
        break;
      }
      case MsgType::Log: {
        // Already a formatted x4 line from the camera; keep it byte-for-byte.
        const char* line = kv.rest();
        x4::log::Level lvl = strstr(line, "lvl=ERROR") ? x4::log::Level::Error : x4::log::Level::Warn;
        logsys::injectRemote(line, strlen(line), lvl);
        break;
      }
      default:
        break;
    }
  }
}

void txTask(void*) {
  ShootJob job;
  char buf[128];
  for (;;) {
    if (xQueueReceive(g_shootQ, &job, portMAX_DELAY) != pdTRUE) continue;
    size_t len = buildShoot(buf, sizeof(buf), job.cmd);
    uint32_t ips[kMaxCams];
    for (uint8_t i = 0; i < kMaxCams; i++) ips[i] = cams::ipOf(i + 1);
    int64_t t0 = esp_timer_get_time();
    for (uint8_t r = 0; r < job.repeat; r++) {
      // Broadcast reaches everyone in one frame; unicast copies are not delayed by stations in
      // power-save (the AP buffers broadcasts until DTIM when any station sleeps, e.g. a phone).
      sendTo(IPADDR_BROADCAST, buf, len);
      for (uint8_t i = 0; i < kMaxCams; i++)
        if (ips[i]) sendTo(ips[i], buf, len);
      if (r + 1 < job.repeat) vTaskDelay(pdMS_TO_TICKS(8));
    }
    X4_LOGI("net", "shoot_sent", "shot=%lu at_us=%lld repeat=%u send_us=%lld", (unsigned long)job.cmd.shot,
            (long long)job.cmd.atUs, job.repeat, (long long)(esp_timer_get_time() - t0));
  }
}

}  // namespace

bool beginUdp(const x4::cfg::Config&) {
  g_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (g_sock < 0) return false;
  int yes = 1;
  setsockopt(g_sock, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
  sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(kUdpCorePort);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(g_sock, (sockaddr*)&addr, sizeof(addr)) < 0) return false;
  g_ackQ = xQueueCreate(16, sizeof(ShotAck));
  g_shootQ = xQueueCreate(4, sizeof(ShootJob));
  // Priorities: tx above everything app-level so a shutter press goes out immediately.
  xTaskCreatePinnedToCore(rxTask, "udp_rx", 4096, nullptr, 6, nullptr, 0);
  xTaskCreatePinnedToCore(txTask, "udp_tx", 3072, nullptr, 7, nullptr, 0);
  X4_LOGI("net", "udp", "rx_port=%u tx_port=%u", kUdpCorePort, kUdpCamPort);
  return true;
}

void sendShoot(const ShootCmd& cmd, uint8_t repeat) {
  ShootJob j{cmd, repeat ? repeat : (uint8_t)1};
  if (g_shootQ) xQueueSend(g_shootQ, &j, 0);
}

void sendCmd(uint8_t cam, const char* cmd, const char* arg) {
  if (g_sock < 0) return;
  char buf[128];
  size_t len = buildCmd(buf, sizeof(buf), cmd, arg);
  uint32_t ip = cam ? cams::ipOf(cam) : IPADDR_BROADCAST;
  if (ip) sendTo(ip, buf, len);
}

bool nextAck(ShotAck* out) { return g_ackQ && xQueueReceive(g_ackQ, out, 0) == pdTRUE; }

}  // namespace net
}  // namespace x4core
