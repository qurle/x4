#include "netcam.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <esp_http_server.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <lwip/sockets.h>

#include "cam.h"
#include "camsys.h"
#include "x4/log.h"
#include "x4/version.h"
#include "x4_secrets.h"

namespace x4cam {
namespace net {
namespace {

using namespace x4::proto;

int g_sock = -1;
sockaddr_in g_core = {};
ClockSync g_clock;
SemaphoreHandle_t g_clockMu = nullptr;
volatile bool g_registered = false;
uint32_t g_lastRegTry = 0, g_regBackoff = 1000;
uint32_t g_hbSeq = 0;
uint32_t g_lastShot = 0;
QueueHandle_t g_shootQ = nullptr;
httpd_handle_t g_http = nullptr, g_stream = nullptr;
bool g_ota = false;

struct ShootJob {
  ShootCmd cmd;
  int64_t rxLocalUs;
};

#if X4_CAM_HELLO
constexpr bool kHello = true;
#else
constexpr bool kHello = false;
#endif

int64_t toCore(int64_t local) {
  xSemaphoreTake(g_clockMu, portMAX_DELAY);
  int64_t v = g_clock.valid() ? g_clock.toCore(local) : 0;
  xSemaphoreGive(g_clockMu);
  return v;
}

int64_t toLocal(int64_t core, bool* ok) {
  xSemaphoreTake(g_clockMu, portMAX_DELAY);
  *ok = g_clock.valid();
  int64_t v = g_clock.toLocal(core);
  xSemaphoreGive(g_clockMu);
  return v;
}

void sendCore(const char* buf, size_t len) {
  if (g_sock >= 0) sendto(g_sock, buf, len, 0, (sockaddr*)&g_core, sizeof(g_core));
}

// ---- UDP receive: SHOOT, HBA, CMD ------------------------------------------------------------
void rxTask(void*) {
  char buf[256];
  for (;;) {
    sockaddr_in from = {};
    socklen_t fl = sizeof(from);
    int n = recvfrom(g_sock, buf, sizeof(buf) - 1, 0, (sockaddr*)&from, &fl);
    int64_t t3 = esp_timer_get_time();
    if (n <= 0) continue;
    buf[n] = 0;
    KvLine kv;
    if (!kv.parse(buf, (size_t)n) || kv.version() != X4_PROTOCOL_VERSION) continue;
    switch (kv.type()) {
      case MsgType::Shoot: {
        ShootJob j;
        if (!parseShoot(kv, &j.cmd)) break;
        if (j.cmd.shot == g_lastShot) break;  // repeats of the same shot
        g_lastShot = j.cmd.shot;
        j.rxLocalUs = t3;
        xQueueSend(g_shootQ, &j, 0);
        break;
      }
      case MsgType::HbAck: {
        HbAck a;
        if (!parseHbAck(kv, &a)) break;
        xSemaphoreTake(g_clockMu, portMAX_DELAY);
        g_clock.addSample(a.t1Us, a.t2Us, t3);
        xSemaphoreGive(g_clockMu);
        break;
      }
      case MsgType::Cmd: {
        const char* c = kv.get("cmd");
        if (c && strcmp(c, "reboot") == 0) {
          X4_LOGW("net", "cmd", "cmd=reboot");
          delay(50);
          ESP.restart();
        }
        break;
      }
      default:
        break;
    }
  }
}

// ---- shooter: highest app priority, pinned away from WiFi ---------------------------------------
void shooterTask(void*) {
  ShootJob j;
  char out[200];
  for (;;) {
    if (xQueueReceive(g_shootQ, &j, portMAX_DELAY) != pdTRUE) continue;
    bool synced = false;
    int64_t localAt = j.cmd.atUs ? toLocal(j.cmd.atUs, &synced) : 0;
    int64_t now = esp_timer_get_time();
    if (!synced || localAt < now || localAt > now + 2000000) {
      // No clock sync yet, already late, or absurd: shoot immediately and say so.
      if (j.cmd.atUs) X4_LOGW("shoot", "unscheduled", "shot=%lu synced=%d late_us=%lld", (unsigned long)j.cmd.shot, synced,
                              (long long)(now - localAt));
      localAt = 0;
    }
    cam::ShotResult r = cam::shoot(j.cmd.shot, localAt, j.cmd.res, j.rxLocalUs);
    ShotAck a{sys::camId(), r.shot, (uint8_t)r.ok, toCore(r.rxLocalUs), toCore(r.capLocalUs), r.bytes, r.capMs, r.err};
    size_t len = buildShotAck(out, sizeof(out), a);
    sendCore(out, len);
    sendCore(out, len);  // ACKs are tiny: send twice instead of waiting for a retry
    X4_LOG(r.ok ? x4::log::Level::Info : x4::log::Level::Error, "shoot", "captured",
           "shot=%lu ok=%d bytes=%lu cap_ms=%lu vs_plan_us=%lld rx_lead_us=%lld err=%u", (unsigned long)r.shot, r.ok,
           (unsigned long)r.bytes, (unsigned long)r.capMs, (long long)(localAt ? r.capLocalUs - localAt : 0),
           (long long)(localAt ? localAt - r.rxLocalUs : 0), r.err);
  }
}

// ---- heartbeat + log forwarding ---------------------------------------------------------------
void hbTask(void*) {
  char out[200];
  char line[200];
  for (;;) {
    const auto& c = conf::get();
    if (WiFi.isConnected() && !kHello) {
      Heartbeat h{sys::camId(), ++g_hbSeq, esp_timer_get_time(), (uint32_t)ESP.getFreeHeap(), (uint32_t)ESP.getFreePsram(),
                  (int16_t)sys::tempC(), (uint8_t)cam::state(), (int8_t)WiFi.RSSI(), cam::heldShot()};
      size_t len = buildHeartbeat(out, sizeof(out), h);
      sendCore(out, len);
      while (sys::popForward(line, sizeof(line))) {
        len = buildLogFwd(out, sizeof(out), sys::camId(), line);
        if (len) sendCore(out, len);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(c.hb_period_ms));
  }
}

// ---- registration -----------------------------------------------------------------------------
bool doRegister() {
  const auto& id = cam::identity();
  JsonDocument d;
  d["id"] = sys::camId();
  d["proto"] = X4_PROTOCOL_VERSION;
  d["fw"] = X4_FW_VERSION;
  d["git"] = X4_GIT_HASH;
  d["mac"] = WiFi.macAddress();
  d["ip"] = WiFi.localIP().toString();
  d["sensor_pid"] = id.pid;
  d["sensor"] = id.sensorName;
  d["hw_rev"] = id.hwRev;
  d["heap"] = ESP.getFreeHeap();
  d["psram"] = ESP.getFreePsram();
  d["chip_rev"] = ESP.getChipRevision();
  String body;
  serializeJson(d, body);
  HTTPClient http;
  http.setTimeout(2000);
  if (!http.begin(String("http://") + kApIp + kPathRegister)) return false;
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(body);
  String resp = code > 0 ? http.getString() : String();
  http.end();
  if (code != 200) {
    X4_LOG_ERR(Warn, "net", "register", x4::Err::NET_REGISTER_FAIL, "http=%d", code);
    return false;
  }
  JsonDocument r;
  if (!deserializeJson(r, resp)) {
    // Camera-scope config is owned by the Core: apply what it sends (not persisted).
    JsonObject cfg = r["config"];
    int applied = 0;
    for (JsonPair kv : cfg) applied += conf::set(kv.key().c_str(), kv.value().as<int32_t>(), false) ? 1 : 0;
    cam::applyConfig(conf::get());
    X4_LOGI("net", "registered", "core_fw=%s cfg_applied=%d", (const char*)(r["core_fw"] | "?"), applied);
  }
  return true;
}

// ---- HTTP (port 80) ---------------------------------------------------------------------------
void cors(httpd_req_t* req) { httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*"); }

int queryInt(httpd_req_t* req, const char* key, int def) {
  char q[96], v[16];
  if (httpd_req_get_url_query_str(req, q, sizeof(q)) != ESP_OK) return def;
  if (httpd_query_key_value(q, key, v, sizeof(v)) != ESP_OK) return def;
  return atoi(v);
}

esp_err_t sendJson(httpd_req_t* req, JsonDocument& d) {
  String s;
  serializeJson(d, s);
  cors(req);
  httpd_resp_set_type(req, "application/json");
  return httpd_resp_send(req, s.c_str(), s.length());
}

esp_err_t hInfo(httpd_req_t* req) {
  const auto& id = cam::identity();
  JsonDocument d;
  d["id"] = sys::camId();
  d["fw"] = X4_FW_VERSION;
  d["git"] = X4_GIT_HASH;
  d["proto"] = X4_PROTOCOL_VERSION;
  d["mac"] = WiFi.macAddress();
  d["ip"] = WiFi.localIP().toString();
  d["rssi"] = WiFi.RSSI();
  d["uptime_ms"] = millis();
  d["reset"] = sys::resetName();
  d["boots"] = sys::bootCount();
  d["chip_rev"] = ESP.getChipRevision();
  d["flash_mb"] = ESP.getFlashChipSize() >> 20;
  d["psram_total"] = ESP.getPsramSize();
  d["psram_free"] = ESP.getFreePsram();
  d["heap_free"] = ESP.getFreeHeap();
  d["temp_c"] = sys::tempC();
  JsonObject s = d["sensor"].to<JsonObject>();
  s["name"] = id.sensorName;
  s["pid"] = id.pid;
  s["ok"] = id.sensorOk;
  s["hw_rev"] = id.hwRev;
  s["module_fw"] = id.fwVer;
  d["state"] = camStateName(cam::state());
  d["held_shot"] = cam::heldShot();
  d["registered"] = (bool)g_registered;
  JsonObject ck = d["clock"].to<JsonObject>();
  ck["synced"] = g_clock.valid();
  ck["offset_us"] = g_clock.offsetUs();
  ck["rtt_us"] = g_clock.rttUs();
  char buf[900];
  x4::cfg::toJson(conf::get(), x4::cfg::CAM, buf, sizeof(buf));
  JsonDocument c;
  deserializeJson(c, buf);
  d["config"] = c;
  return sendJson(req, d);
}

esp_err_t hHealth(httpd_req_t* req) {
  JsonDocument d;
  d["ok"] = cam::identity().sensorOk;
  d["state"] = camStateName(cam::state());
  d["heap"] = ESP.getFreeHeap();
  d["temp_c"] = sys::tempC();
  return sendJson(req, d);
}

esp_err_t sendFrame(httpd_req_t* req, const uint8_t* buf, size_t len, const char* extraKey, const char* extraVal) {
  cors(req);
  httpd_resp_set_type(req, "image/jpeg");
  if (extraKey) httpd_resp_set_hdr(req, extraKey, extraVal);
  // Send in 16 KB pieces so one big frame does not need one huge socket buffer.
  constexpr size_t kPiece = 16 * 1024;
  for (size_t off = 0; off < len; off += kPiece) {
    size_t n = len - off < kPiece ? len - off : kPiece;
    if (httpd_resp_send_chunk(req, (const char*)buf + off, n) != ESP_OK) return ESP_FAIL;
  }
  return httpd_resp_send_chunk(req, nullptr, 0);
}

esp_err_t hCapture(httpd_req_t* req) {
  int res = queryInt(req, "res", conf::get().cam_res);
  if (res < 0 || res > 5) res = 5;
  if (res >= 1 && sys::tempC() >= conf::get().therm_thr_c) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "too hot");
    return ESP_FAIL;
  }
  cam::Frame f;
  if (!cam::grab(res, &f) || !f.jpegOk) {
    cam::release(f);
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "capture failed");
    return ESP_FAIL;
  }
  char ms[12];
  snprintf(ms, sizeof(ms), "%lu", (unsigned long)f.grabMs);
  esp_err_t e = sendFrame(req, f.fb->buf, f.fb->len, "X-Grab-Ms", ms);
  cam::release(f);
  return e;
}

esp_err_t hShot(httpd_req_t* req) {
  uint32_t id = (uint32_t)queryInt(req, "id", 0);
  camera_fb_t* fb = cam::takeHeld(id);
  if (!fb) {
    X4_LOG_ERR(Warn, "http", "shot", x4::Err::CAM_SHOT_EXPIRED, "want=%lu held=%lu", (unsigned long)id,
               (unsigned long)cam::heldShot());
    httpd_resp_set_status(req, "410 Gone");
    cors(req);
    return httpd_resp_send(req, "shot not held", HTTPD_RESP_USE_STRLEN);
  }
  uint32_t t0 = millis();
  char shot[12];
  snprintf(shot, sizeof(shot), "%lu", (unsigned long)id);
  // Content-Length lets the Core verify the download; then the frame is sent in pieces.
  char lenS[12];
  snprintf(lenS, sizeof(lenS), "%u", (unsigned)fb->len);
  cors(req);
  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "X-Shot", shot);
  // httpd_resp_send sets Content-Length; send the whole buffer in one call (it streams it).
  esp_err_t e = httpd_resp_send(req, (const char*)fb->buf, fb->len);
  uint32_t ms = millis() - t0;
  X4_LOGI("http", "shot_sent", "shot=%lu bytes=%s ms=%lu ok=%d", (unsigned long)id, lenS, (unsigned long)ms, e == ESP_OK);
  cam::doneHeld(e == ESP_OK);
  return e;
}

esp_err_t hSettings(httpd_req_t* req) {
  char q[160];
  JsonDocument d;
  JsonArray res = d["results"].to<JsonArray>();
  if (httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK) {
    // key=value&key=value
    char* save = nullptr;
    for (char* kv = strtok_r(q, "&", &save); kv; kv = strtok_r(nullptr, "&", &save)) {
      char* eq = strchr(kv, '=');
      if (!eq) continue;
      *eq = 0;
      JsonObject r = res.add<JsonObject>();
      r["key"] = kv;
      if (strcmp(kv, "cam_id") == 0) {
        int id = atoi(eq + 1);
        if (id >= 1 && id <= 4) sys::setCamId((uint8_t)id);
        r["ok"] = id >= 1 && id <= 4;
        r["reboot"] = true;
        continue;
      }
      bool ok = conf::set(kv, atoi(eq + 1), true);
      r["ok"] = ok;
      if (ok) r["value"] = x4::cfg::get(conf::get(), *x4::cfg::find(kv));
    }
    cam::applyConfig(conf::get());
  }
  return sendJson(req, d);
}

esp_err_t hLogs(httpd_req_t* req) {
  static char buf[8192];
  size_t n = sys::ring().tail(buf, sizeof(buf), 60);
  cors(req);
  httpd_resp_set_type(req, "text/plain");
  return httpd_resp_send(req, buf, n);
}

// ---- MJPEG stream (port 81) ---------------------------------------------------------------------
esp_err_t hStream(httpd_req_t* req) {
  static const char* kBoundary = "x4frame";
  char hdr[96];
  cors(req);
  httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=x4frame");
  int res = queryInt(req, "res", 0);
  cam::setState(CamState::Streaming);
  X4_LOGI("http", "stream_start", "res=%s", cam::resName(res));
  for (;;) {
    if (sys::tempC() >= conf::get().therm_thr_c) {
      X4_LOG_ERR(Warn, "http", "stream_stop", x4::Err::OVERHEAT, "temp_c=%.1f", sys::tempC());
      break;
    }
    if (cam::heldShot()) {  // a shot is waiting for transfer: keep the buffers free
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }
    cam::Frame f;
    if (!cam::grab(res, &f)) break;
    int hl = snprintf(hdr, sizeof(hdr), "\r\n--%s\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", kBoundary,
                      (unsigned)f.fb->len);
    bool ok = httpd_resp_send_chunk(req, hdr, hl) == ESP_OK &&
              httpd_resp_send_chunk(req, (const char*)f.fb->buf, f.fb->len) == ESP_OK;
    cam::release(f);
    if (!ok) break;
  }
  cam::setState(CamState::Idle);
  X4_LOGI("http", "stream_end", "res=%s", cam::resName(res));
  return ESP_OK;
}

void startHttp() {
  httpd_config_t c = HTTPD_DEFAULT_CONFIG();
  c.server_port = kCamHttpPort;
  c.ctrl_port = 32768;
  c.max_uri_handlers = 12;
  c.stack_size = 8192;
  c.lru_purge_enable = true;
  c.send_wait_timeout = 10;
  if (httpd_start(&g_http, &c) == ESP_OK) {
    httpd_uri_t uris[] = {
        {kPathInfo, HTTP_GET, hInfo, nullptr},         {kPathHealth, HTTP_GET, hHealth, nullptr},
        {kPathCapture, HTTP_GET, hCapture, nullptr},   {kPathShot, HTTP_GET, hShot, nullptr},
        {kPathSettings, HTTP_GET, hSettings, nullptr}, {kPathSettings, HTTP_POST, hSettings, nullptr},
        {kPathLogs, HTTP_GET, hLogs, nullptr},
    };
    for (auto& u : uris) httpd_register_uri_handler(g_http, &u);
  }
  httpd_config_t s = HTTPD_DEFAULT_CONFIG();
  s.server_port = kCamStreamPort;
  s.ctrl_port = 32769;
  s.stack_size = 6144;
  if (httpd_start(&g_stream, &s) == ESP_OK) {
    httpd_uri_t u = {kPathStream, HTTP_GET, hStream, nullptr};
    httpd_register_uri_handler(g_stream, &u);
  }
  X4_LOGI("http", "start", "api_port=%u stream_port=%u", kCamHttpPort, kCamStreamPort);
}

}  // namespace

bool joinWifi() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // modem sleep adds 100+ ms random latency to the trigger
  WiFi.setAutoReconnect(true);
  const auto& c = conf::get();
  const char* ssid = kHello ? X4_HOME_SSID : X4_AP_SSID;
  const char* pass = kHello ? X4_HOME_PASS : X4_AP_PASS;
  uint32_t t0 = millis();
  if (kHello) WiFi.begin(ssid, pass);
  else WiFi.begin(ssid, pass, c.ap_channel);  // fixed channel: no full scan
  while (!WiFi.isConnected() && millis() - t0 < 15000) delay(50);
  if (!WiFi.isConnected()) {
    X4_LOG_ERR(Error, "net", "join", x4::Err::NET_JOIN_FAIL, "ssid=%s ms=%lu status=%d", ssid, (unsigned long)(millis() - t0),
               (int)WiFi.status());
    return false;
  }
  esp_wifi_set_max_tx_power((int8_t)c.wifi_tx_qdbm);
  X4_LOGI("net", "joined", "ssid=%s ip=%s rssi=%d ch=%d ms=%lu", ssid, WiFi.localIP().toString().c_str(), WiFi.RSSI(),
          WiFi.channel(), (unsigned long)(millis() - t0));
  return true;
}

void begin() {
  g_clockMu = xSemaphoreCreateMutex();
  g_shootQ = xQueueCreate(2, sizeof(ShootJob));
  g_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  int yes = 1;
  setsockopt(g_sock, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
  sockaddr_in a = {};
  a.sin_family = AF_INET;
  a.sin_port = htons(kUdpCamPort);
  a.sin_addr.s_addr = htonl(INADDR_ANY);
  bind(g_sock, (sockaddr*)&a, sizeof(a));
  g_core.sin_family = AF_INET;
  g_core.sin_port = htons(kUdpCorePort);
  g_core.sin_addr.s_addr = inet_addr(kApIp);
  xTaskCreatePinnedToCore(rxTask, "udp_rx", 4096, nullptr, 6, nullptr, 0);
  xTaskCreatePinnedToCore(shooterTask, "shooter", 6144, nullptr, 10, nullptr, 1);
  xTaskCreatePinnedToCore(hbTask, "hb", 4096, nullptr, 3, nullptr, 0);
  startHttp();
  if (sizeof(X4_OTA_PASS) > 1) {
    char host[16];
    snprintf(host, sizeof(host), "x4-cam%u", sys::camId());
    ArduinoOTA.setHostname(host);
    ArduinoOTA.setPassword(X4_OTA_PASS);
    ArduinoOTA.onStart([]() { X4_LOGW("ota", "start", "cam=%u", sys::camId()); });
    ArduinoOTA.begin();
    g_ota = true;
  }
  sys::enableForward(!kHello);
}

void poll(uint32_t now) {
  if (g_ota) ArduinoOTA.handle();
  cam::expireHeld(now, (uint32_t)conf::get().cam_hold_ms);
  if (kHello) return;
  if (!WiFi.isConnected()) {
    if (g_registered) X4_LOG_ERR(Warn, "net", "lost", x4::Err::NET_JOIN_FAIL, "rssi=%d", WiFi.RSSI());
    g_registered = false;
    return;
  }
  if (!g_registered && now - g_lastRegTry >= g_regBackoff) {
    g_lastRegTry = now;
    if (sys::camId() == 0) {
      X4_LOG_EVERY(10000, x4::log::Level::Error, "net", "no_id", "set id: flash env camN or GET /settings?cam_id=N");
      return;
    }
    g_registered = doRegister();
    g_regBackoff = g_registered ? 1000 : (g_regBackoff < 8000 ? g_regBackoff * 2 : 8000);
  }
}

bool registered() { return g_registered; }
const ClockSync& clock() { return g_clock; }
int8_t rssi() { return (int8_t)WiFi.RSSI(); }

}  // namespace net
}  // namespace x4cam
