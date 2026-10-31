// HTTP server on the Core's AP (port 80): the web debug page, the JSON API and /register.
// Same origin as the API, so no CORS or mixed-content trouble (docs/COMPANION.md).
// Runs in its own task; talks to the app only through status::get(), app::request(), conf::*.
#include <ArduinoJson.h>
#include <SD.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_heap_caps.h>

#include "../app/app.h"
#include "../app/config_store.h"
#include "../app/status.h"
#include "../hw/spi_lock.h"
#include "../input/inputs.h"
#include "../log/log_setup.h"
#include "../storage/sd_store.h"
#include "cams.h"
#include "net.h"
#include "web_page.h"
#include "x4/log.h"
#include "x4/shoot.h"
#include "x4/version.h"

namespace x4core {
namespace net {
namespace {

WebServer g_srv(80);

void cors() {
  g_srv.sendHeader("Access-Control-Allow-Origin", "*");
  g_srv.sendHeader("Cache-Control", "no-store");
}

void sendJson(int code, JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  cors();
  g_srv.send(code, "application/json", out);
}

void sendErr(int code, x4::Err e, const char* detail) {
  JsonDocument d;
  d["ok"] = false;
  d["err"] = (int)e;
  d["err_name"] = x4::errName(e);
  d["detail"] = detail;
  sendJson(code, d);
}

void hIndex() {
  g_srv.sendHeader("Cache-Control", "no-store");
  g_srv.send_P(200, "text/html", kWebPage);
}

void hStatus() {
  Status s = status::get();
  JsonDocument d;
  d["fw"] = X4_FW_VERSION;
  d["git"] = X4_GIT_HASH;
  d["proto"] = X4_PROTOCOL_VERSION;
  d["state"] = x4::shoot::stateName(s.state);
  d["faults"] = s.faults;
  d["uptime_ms"] = s.uptimeMs;
  d["shots_taken"] = s.shotsTaken;
  d["cams_online"] = s.camsOnline;
  d["ap_stations"] = s.apStations;
  JsonObject sh = d["last_shot"].to<JsonObject>();
  sh["id"] = s.lastShotId;
  sh["total_ms"] = s.lastTotalMs;
  sh["captured_ms"] = s.lastCapturedMs;
  sh["skew_us"] = s.lastSkewUs;
  sh["stored"] = s.lastStored;
  sh["expected"] = s.lastExpected;
  JsonArray cams = d["cams"].to<JsonArray>();
  for (int i = 0; i < x4::proto::kMaxCams; i++) {
    const CamView& c = s.cam[i];
    JsonObject o = cams.add<JsonObject>();
    o["id"] = i + 1;
    o["registered"] = c.registered;
    o["online"] = c.online;
    o["ip"] = c.ip;
    o["fw"] = c.fw;
    o["sensor"] = c.sensorPid;
    o["hw_rev"] = c.hwRev;
    o["temp_c"] = c.tempC;
    o["rssi"] = c.rssi;
    o["heap"] = c.heap;
    o["psram"] = c.psram;
    o["state"] = x4::proto::camStateName((x4::proto::CamState)c.state);
    o["hb_age_ms"] = c.lastHbAgeMs;
    o["shot_ok"] = c.ok;
    o["shot_stored"] = c.stored;
    o["shot_failed"] = c.failed;
    o["bytes"] = c.bytes;
    o["xfer_bytes"] = c.xferBytes;
    o["xfer_total"] = c.xferTotal;
    o["cap_offset_us"] = c.capOffsetUs;
    o["cap_ms"] = c.capMs;
  }
  JsonObject fl = d["flash"].to<JsonObject>();
  fl["enabled"] = s.flashEnabled;
  fl["armed"] = s.flashArmed;
  fl["ready"] = s.flashReady;
  fl["ready_in_ms"] = s.flashReadyInMs;
  fl["pulses"] = s.flashPulses;
  JsonObject pw = d["power"].to<JsonObject>();
  pw["safe"] = s.powerSafe;
  pw["bus_out_en"] = s.busOutEn;
  pw["usb_otg_en"] = s.usbOtgEn;
  pw["vbus_mv"] = s.vbusMv;
  pw["battery"] = "none (CoreS3-SE): power bank level is not readable";
  JsonObject sy = d["system"].to<JsonObject>();
  sy["temp_c"] = s.tempC;
  sy["hot"] = s.hot;
  sy["overheat"] = s.overheat;
  sy["heap"] = s.heap;
  sy["heap_min"] = s.heapMin;
  sy["psram"] = s.psram;
  sy["reset"] = s.resetReason;
  sy["boots"] = s.bootCount;
  sy["brownouts"] = s.brownouts;
  JsonObject sd = d["sd"].to<JsonObject>();
  sd["mounted"] = s.sdMounted;
  sd["free_mb"] = s.sdFreeMb;
  sd["total_mb"] = s.sdTotalMb;
  sd["write_kBps"] = s.sdWriteKBps;
  sd["read_kBps"] = s.sdReadKBps;
  JsonObject ui = d["ui"].to<JsonObject>();
  ui["fps"] = s.fps;
  ui["frame_max_us"] = s.frameMaxUs;
  ui["preview_on"] = s.previewOn;
  ui["preview_fps"] = s.previewFps;
  ui["button_down"] = s.buttonDown;
  sendJson(200, d);
}

void hLogs() {
  uint32_t since = g_srv.hasArg("since") ? (uint32_t)g_srv.arg("since").toInt() : 0;
  String out;
  out.reserve(4096);
  uint32_t last = logsys::ring().forEachSince(since, [&](uint32_t, const char* t) {
    if (out.length() < 12000) {
      out += t;
      out += '\n';
    }
  });
  cors();
  g_srv.sendHeader("X-Log-Seq", String(last));
  g_srv.send(200, "text/plain", out);
}

void hConfigGet() {
  JsonDocument d;
  const auto& c = conf::get();
  const auto& b = conf::base();
  d["version"] = x4::cfg::kConfigVersion;
  JsonArray arr = d["options"].to<JsonArray>();
  for (size_t i = 0; i < x4::cfg::optionCount(); i++) {
    const auto& o = x4::cfg::option(i);
    JsonObject j = arr.add<JsonObject>();
    j["name"] = o.name;
    j["scope"] = o.scope == x4::cfg::BOTH ? "both" : (o.scope & x4::cfg::CAM) ? "cam" : "core";
    j["value"] = x4::cfg::get(c, o);
    j["default"] = x4::cfg::get(b, o);
    j["min"] = o.min;
    j["max"] = o.max;
    j["unit"] = o.unit;
    j["safety"] = (o.flags & x4::cfg::SAFE_MAX) ? "max" : (o.flags & x4::cfg::SAFE_MIN) ? "min" : "";
    j["reboot"] = (o.flags & x4::cfg::REBOOT) != 0;
    j["doc"] = o.doc;
  }
  sendJson(200, d);
}

// POST /config?key=value[&key2=value2]  (form or query args)
void hConfigPost() {
  JsonDocument d;
  JsonArray res = d["results"].to<JsonArray>();
  bool any = false;
  for (int i = 0; i < g_srv.args(); i++) {
    String k = g_srv.argName(i);
    if (k == "plain") continue;
    any = true;
    JsonObject r = res.add<JsonObject>();
    r["key"] = k;
    x4::cfg::Issue is;
    if (!conf::set(k.c_str(), (int32_t)g_srv.arg(i).toInt(), &is)) {
      r["ok"] = false;
      r["err_name"] = x4::errName(x4::Err::CFG_UNKNOWN_KEY);
      continue;
    }
    r["ok"] = true;
    r["value"] = x4::cfg::get(conf::get(), *x4::cfg::find(k.c_str()));
    if (is.name) {
      r["clamped"] = is.reason;
      r["requested"] = is.requested;
    }
  }
  if (!any) return sendErr(400, x4::Err::CFG_UNKNOWN_KEY, "no key=value given");
  sendJson(200, d);
}

void hRegister() {
  JsonDocument in;
  if (deserializeJson(in, g_srv.arg("plain"))) return sendErr(400, x4::Err::NET_REGISTER_FAIL, "bad json");
  int id = in["id"] | 0;
  int proto = in["proto"] | 0;
  if (proto != X4_PROTOCOL_VERSION) {
    X4_LOG_ERR(Warn, "net", "register", x4::Err::NET_PROTO_MISMATCH, "cam=%d proto=%d", id, proto);
    return sendErr(409, x4::Err::NET_PROTO_MISMATCH, "protocol version mismatch");
  }
  uint32_t ip = (uint32_t)g_srv.client().remoteIP();
  if (!cams::registerCam((uint8_t)id, ip, in["fw"] | "", in["git"] | "", in["mac"] | "",
                         (uint16_t)(in["sensor_pid"] | 0), (uint8_t)(in["hw_rev"] | 0), millis()))
    return sendErr(400, x4::Err::NET_REGISTER_FAIL, "id must be 1..4");
  X4_LOGI("net", "register_info", "cam=%d heap=%lu psram=%lu chip_rev=%d", id, (unsigned long)(in["heap"] | 0),
          (unsigned long)(in["psram"] | 0), (int)(in["chip_rev"] | 0));
  JsonDocument out;
  out["ok"] = true;
  out["core_fw"] = X4_FW_VERSION;
  out["proto"] = X4_PROTOCOL_VERSION;
  // Cameras take their runtime options from here (camera-scope config owned by the Core).
  JsonObject cfg = out["config"].to<JsonObject>();
  const auto& c = conf::get();
  for (size_t i = 0; i < x4::cfg::optionCount(); i++) {
    const auto& o = x4::cfg::option(i);
    if (o.scope & x4::cfg::CAM) cfg[o.name] = x4::cfg::get(c, o);
  }
  sendJson(200, out);
}

void hAction() {
  String a = g_srv.arg("a");
  if (a == "shutter") inputs::inject(x4::input::Source::Web, x4::input::Gesture::Click);
  else if (a == "flash_arm") app::request(app::Request::FlashArm);
  else if (a == "flash_disarm") app::request(app::Request::FlashDisarm);
  else if (a == "flash_test") app::request(app::Request::FlashTest);
  else if (a == "sd_bench") app::request(app::Request::SdBench);
  else if (a == "preview_on") app::request(app::Request::PreviewOn);
  else if (a == "preview_off") app::request(app::Request::PreviewOff);
  else if (a == "reboot") app::request(app::Request::Reboot);
  else return sendErr(400, x4::Err::INTERNAL, "unknown action");
  X4_LOGI("web", "action", "a=%s from=%s", a.c_str(), g_srv.client().remoteIP().toString().c_str());
  JsonDocument d;
  d["ok"] = true;
  d["action"] = a;
  sendJson(200, d);
}

void hShots() {
  char names[24][8];
  int n = sd::listShots(names, 24);
  JsonDocument d;
  JsonArray arr = d["shots"].to<JsonArray>();
  for (int i = 0; i < n; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["id"] = names[i];
    JsonArray files = o["files"].to<JsonArray>();
    char dir[40];
    snprintf(dir, sizeof(dir), "/x4/shots/%s", names[i]);
    SpiLock l;
    File f = SD.open(dir);
    if (!f) continue;
    for (File e = f.openNextFile(); e; e = f.openNextFile()) {
      JsonObject fo = files.add<JsonObject>();
      fo["name"] = e.name();
      fo["size"] = (uint32_t)e.size();
      e.close();
    }
    f.close();
  }
  sendJson(200, d);
}

// GET /file?path=/x4/shots/S000001/cam1.jpg  (only below /x4)
void hFile() {
  String p = g_srv.arg("path");
  if (!p.startsWith("/x4/") || p.indexOf("..") >= 0) return sendErr(403, x4::Err::INTERNAL, "path not allowed");
  File f;
  {
    SpiLock l;
    f = SD.open(p, FILE_READ);
  }
  if (!f) return sendErr(404, x4::Err::INTERNAL, "not found");
  size_t size = f.size();
  const char* type = p.endsWith(".jpg") ? "image/jpeg" : p.endsWith(".json") ? "application/json" : "text/plain";
  cors();
  g_srv.setContentLength(size);
  g_srv.send(200, type, "");
  uint8_t* buf = (uint8_t*)heap_caps_malloc(8192, MALLOC_CAP_SPIRAM);
  WiFiClient cl = g_srv.client();
  while (buf && size) {
    int n;
    {
      SpiLock l;  // short holds so the UI keeps flushing
      n = f.read(buf, size < 8192 ? size : 8192);
    }
    if (n <= 0) break;
    if (cl.write(buf, n) != (size_t)n) break;
    size -= (size_t)n;
  }
  heap_caps_free(buf);
  SpiLock l;
  f.close();
}

void webTask(void*) {
  for (;;) {
    g_srv.handleClient();
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

}  // namespace

bool beginWeb() {
  g_srv.on("/", HTTP_GET, hIndex);
  g_srv.on("/api/status", HTTP_GET, hStatus);
  g_srv.on("/logs", HTTP_GET, hLogs);
  g_srv.on("/config", HTTP_GET, hConfigGet);
  g_srv.on("/config", HTTP_POST, hConfigPost);
  g_srv.on("/register", HTTP_POST, hRegister);
  g_srv.on("/api/action", HTTP_POST, hAction);
  g_srv.on("/api/shots", HTTP_GET, hShots);
  g_srv.on("/file", HTTP_GET, hFile);
  g_srv.onNotFound([]() { sendErr(404, x4::Err::INTERNAL, "no such endpoint"); });
  g_srv.begin();
  xTaskCreatePinnedToCore(webTask, "web", 8192, nullptr, 3, nullptr, 0);
  X4_LOGI("web", "start", "port=80 url=http://192.168.4.1/");
  return true;
}

}  // namespace net
}  // namespace x4core
