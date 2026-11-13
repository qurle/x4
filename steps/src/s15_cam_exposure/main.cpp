// Step 15: exposure test on one camera. Can the camera freeze motion indoors?
// The camera module (PY260 behind M5's "MEGA_CCM" controller) runs its own auto exposure; in dim
// light it picks long exposures and everything that moves blurs.
//
// What we know about its controls, and from where:
// - Upstream esp32-camera (the library we build with) maps only a few of them and its exposure
//   setter writes the HIGH byte into BOTH exposure registers (bug, VERIFIED in
//   sensors/mega_ccm.c). So this firmware writes registers itself with the driver's exported
//   SCCB_Write16.
// - A third-party driver that was worked out on this unit (github.com/hbentel/M5Stack-Unit-CamS3-5MP,
//   components/esp32-camera/sensors/mega_ccm.c + its CLAUDE.md/README) gives the meanings used
//   below. Third-party, so tagged "per hbentel", not M5-official:
//     0x0125 exposure compensation, levels 0..6 = 0, +1, -1, +2, -2, +3, -3
//     0x0130 AGC mode: 0 = auto, 1 = manual         0x0131 manual gain (its driver caps at 6)
//     0x0132 "AE mode" (defined, never used there: meaning Not sure)
//     0x0133 / 0x0134 manual exposure high / low byte (units Not sure)
//   and two warnings this firmware follows:
//     * NO register READS while the camera streams: they make it fall back to 320x240 or hang
//       its control bus. So nothing here reads registers; it reports what it WROTE, and every
//       shot checks the real frame size instead.
//     * Writes right after a sensor reset can be acknowledged but ignored. If a setting seems to
//       do nothing just after boot, wait a minute and Apply again.
// - Frame rate scales with the sensor clock (per hbentel: VGA ~9.4 / 15 / 18.8 fps at 10 / 16 /
//   20 MHz), "and higher clocks shorten per-frame exposure". So the clock is a second lever on
//   blur: this page can restart the camera at another clock.
//
// Wiring: camera on M5's Grove-to-USB-C adapter only, like step 8.
// Use: join WiFi "x4cam-exposure" (password x4cam-steps), open http://192.168.4.1/
// If the camera stops answering: unplug and replug it (all settings reset).
#include <WebServer.h>
#include <WiFi.h>

#include "cam_common.h"

// From the esp32-camera driver (private header sccb.h; the function is exported by the library).
extern "C" int SCCB_Write16(uint8_t slv_addr, uint16_t reg, uint8_t data);

constexpr uint8_t kSccbAddr = 0x1F;  // MEGA_CCM SCCB address (cam_common.h, VERIFIED)
constexpr uint16_t kRegComp = 0x0125;
constexpr uint16_t kRegAgcMode = 0x0130;
constexpr uint16_t kRegGain = 0x0131;
constexpr uint16_t kRegAeMode = 0x0132;
constexpr uint16_t kRegExpH = 0x0133;
constexpr uint16_t kRegExpL = 0x0134;

WebServer server(80);
uint32_t g_shots = 0;
int g_xclkMhz = 20;
framesize_t g_size = FRAMESIZE_VGA;

// What we last wrote (-1 = never written since boot: the camera's own default).
struct Written {
  int comp = -1, agc = -1, gain = -1, ae = -1;
  long exp = -1;
} g_w;

const char* sizeName(framesize_t f) {
  switch (f) {
    case FRAMESIZE_VGA: return "vga";
    case FRAMESIZE_HD: return "hd";
    case FRAMESIZE_UXGA: return "uxga";
    case FRAMESIZE_FHD: return "fhd";
    default: return "5mp";
  }
}

framesize_t sizeFor(const String& s) {
  if (s == "vga") return FRAMESIZE_VGA;
  if (s == "hd") return FRAMESIZE_HD;
  if (s == "uxga") return FRAMESIZE_UXGA;
  if (s == "fhd") return FRAMESIZE_FHD;
  return FRAMESIZE_5MP;
}

int expectedWidth(framesize_t f) {
  switch (f) {
    case FRAMESIZE_VGA: return 640;
    case FRAMESIZE_HD: return 1280;
    case FRAMESIZE_UXGA: return 1600;
    case FRAMESIZE_FHD: return 1920;
    default: return 2592;
  }
}

String writtenJson() {
  char b[200];
  snprintf(b, sizeof(b), "{\"xclk_mhz\":%d,\"size\":\"%s\",\"comp\":%d,\"agc\":%d,\"gain\":%d,\"ae\":%d,\"exp\":%ld}",
           g_xclkMhz, sizeName(g_size), g_w.comp, g_w.agc, g_w.gain, g_w.ae, g_w.exp);
  return String(b);
}

// Time between consecutive frames. Frames can't be shorter than the exposure, so if a long
// manual exposure is honoured this usually grows (ASSUMPTION for this module: test it).
uint32_t framePeriodMs(int frames = 4) {
  camera_fb_t* fb = esp_camera_fb_get();
  if (fb) esp_camera_fb_return(fb);
  uint32_t t0 = millis();
  int got = 0;
  for (int i = 0; i < frames; i++) {
    fb = esp_camera_fb_get();
    if (!fb) break;
    esp_camera_fb_return(fb);
    got++;
  }
  return got ? (millis() - t0) / got : 0;
}

bool startCamera() {
  uint16_t pid = cameraBegin(g_size, g_xclkMhz * 1000000);
  if (pid == 0) return false;
  cameraWarmUp();
  g_w = Written{};  // a fresh camera has its own defaults again
  Serial.printf("exp camera xclk_mhz=%d size=%s pid=0x%04x\n", g_xclkMhz, sizeName(g_size), pid);
  return true;
}

void handleRoot() {
  static const char kPage[] PROGMEM = R"HTML(<!doctype html><html><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1"><title>x4cam step 15</title>
<style>body{font:15px system-ui;margin:12px;background:#111;color:#eee}input,select,button{font:inherit;margin:2px}
input[type=number]{width:6em}img{max-width:100%;margin-top:8px}table{border-collapse:collapse;font:12px monospace}
td,th{border:1px solid #444;padding:2px 5px}pre{white-space:pre-wrap}fieldset{border:1px solid #444;margin:8px 0}
.mu{color:#999;font-size:13px}</style></head><body>
<h2>x4cam step 15: exposure</h2>
<p class=mu>Only ticked fields are written. Nothing is read back from the camera (reads while it
streams break it), so "written" shows what we sent. Just after boot a write can be ignored: wait a
minute and Apply again.</p>
<fieldset><legend>1. easiest: exposure compensation (0x0125)</legend>
<label><input id=wcomp type=checkbox> write</label>
<select id=comp><option value=0>0 (default)</option><option value=2>−1</option><option value=4>−2</option>
<option value=6>−3 (darkest, shortest?)</option><option value=1>+1</option><option value=3>+2</option><option value=5>+3</option></select>
</fieldset>
<fieldset><legend>2. manual exposure and gain</legend>
<label><input id=wagc type=checkbox> AGC mode (0x0130)</label>
<select id=agc><option value=0>0 auto</option><option value=1>1 manual</option></select>
<label><input id=wgain type=checkbox> gain (0x0131)</label> <input id=gain type=number min=0 max=255 value=0>
<span class=mu>(0–6 in hbentel's driver)</span><br>
<label><input id=wae type=checkbox> AE mode (0x0132, meaning Not sure)</label> <input id=ae type=number min=0 max=255 value=1><br>
<label><input id=wexp type=checkbox> exposure (0x0133/34)</label> <input id=exp type=number min=0 max=65535 value=1000>
<span class=mu>(units Not sure)</span>
</fieldset>
<fieldset><legend>3. size and sensor clock</legend>
size <select id=res><option value=vga selected>VGA</option><option value=hd>HD</option><option value=uxga>UXGA</option>
<option value=fhd>FHD</option><option value=5mp>5 MP</option></select>
clock <select id=xclk><option value=10>10 MHz</option><option value=16>16 MHz</option><option value=20 selected>20 MHz</option></select>
<span class=mu>(a new clock restarts the camera: all register settings reset)</span>
</fieldset>
<p><button onclick=apply()>Apply</button> <button onclick=shoot()>Shoot</button></p>
<pre id=out></pre>
<table id=log><tr><th>#</th><th>MHz</th><th>size</th><th>comp</th><th>agc</th><th>gain</th><th>ae</th><th>exp</th><th>frame ms</th><th>grab ms</th><th>kB</th><th>size ok</th></tr></table>
<img id=img>
<script>
const $=id=>document.getElementById(id);
function q(){let p=new URLSearchParams({res:$('res').value,xclk:$('xclk').value});
 for(const k of ['comp','agc','gain','ae','exp']) if($('w'+k).checked) p.set(k,$(k).value); return p}
async function apply(){$('out').textContent='applying...';const r=await fetch('/set?'+q());$('out').textContent=(r.ok?'written: ':'write error: ')+await r.text()}
async function shoot(){$('out').textContent='shooting...';
 const r=await fetch('/shot');if(!r.ok){$('out').textContent='shot failed: '+await r.text();return}
 const h=k=>r.headers.get('X-'+k);const b=await r.blob();$('img').src=URL.createObjectURL(b);
 const row=$('log').insertRow(1);
 [h('Shot'),h('Xclk'),h('Res'),h('Comp'),h('Agc'),h('Gain'),h('Ae'),h('Exp'),h('Frame-Ms'),h('Grab-Ms'),(b.size/1024).toFixed(0),h('Size-Ok')].forEach(v=>row.insertCell().textContent=v);
 $('out').textContent=h('Size-Ok')=='yes'?'ok':'camera fell back to a different size: Apply again, or replug'}
</script></body></html>)HTML";
  server.send_P(200, "text/html", kPage);
}

void handleSet() {
  // Clock change = restart the camera (registers reset to the camera's defaults).
  int xclk = server.hasArg("xclk") ? server.arg("xclk").toInt() : g_xclkMhz;
  framesize_t size = server.hasArg("res") ? sizeFor(server.arg("res")) : g_size;
  if (xclk != g_xclkMhz && (xclk == 10 || xclk == 16 || xclk == 20)) {
    esp_camera_deinit();
    g_xclkMhz = xclk;
    g_size = size;
    if (!startCamera()) {
      server.send(500, "text/plain", "camera restart failed: replug the camera");
      return;
    }
  } else if (size != g_size) {
    sensor_t* s = esp_camera_sensor_get();
    if (s && s->set_framesize(s, size) == 0) g_size = size;
    cameraWarmUp(2);
  }

  int err = 0;
  // Order: compensation, then modes, then the manual values.
  if (server.hasArg("comp")) {
    g_w.comp = server.arg("comp").toInt();
    err += SCCB_Write16(kSccbAddr, kRegComp, (uint8_t)g_w.comp);
  }
  if (server.hasArg("agc")) {
    g_w.agc = server.arg("agc").toInt();
    err += SCCB_Write16(kSccbAddr, kRegAgcMode, (uint8_t)g_w.agc);
  }
  if (server.hasArg("ae")) {
    g_w.ae = server.arg("ae").toInt();
    err += SCCB_Write16(kSccbAddr, kRegAeMode, (uint8_t)g_w.ae);
  }
  if (server.hasArg("gain")) {
    g_w.gain = server.arg("gain").toInt();
    err += SCCB_Write16(kSccbAddr, kRegGain, (uint8_t)g_w.gain);
  }
  if (server.hasArg("exp")) {
    g_w.exp = server.arg("exp").toInt();
    uint16_t e = (uint16_t)g_w.exp;
    err += SCCB_Write16(kSccbAddr, kRegExpH, e >> 8);
    err += SCCB_Write16(kSccbAddr, kRegExpL, e & 0xFF);
  }
  cameraWarmUp(2);  // new settings show up a frame or two later
  String j = writtenJson();
  Serial.printf("exp set write_err=%d written=%s\n", err, j.c_str());
  server.send(err ? 500 : 200, "application/json", j);
}

void handleShot() {
  uint32_t frameMs = framePeriodMs();
  cameraWarmUp(1);
  uint32_t t0 = millis();
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("exp shot FAILED");
    server.send(500, "text/plain", "capture failed");
    return;
  }
  uint32_t grabMs = millis() - t0;
  bool sizeOk = (int)fb->width == expectedWidth(g_size);
  g_shots++;
  Serial.printf("exp shot n=%lu xclk_mhz=%d size=%s w=%u h=%u size_ok=%d comp=%d agc=%d gain=%d ae=%d exp=%ld "
                "frame_ms=%lu grab_ms=%lu bytes=%u\n",
                (unsigned long)g_shots, g_xclkMhz, sizeName(g_size), fb->width, fb->height, sizeOk, g_w.comp, g_w.agc,
                g_w.gain, g_w.ae, g_w.exp, (unsigned long)frameMs, (unsigned long)grabMs, (unsigned)fb->len);
  auto v = [](long x) { return x < 0 ? String("-") : String(x); };
  server.sendHeader("X-Shot", String(g_shots));
  server.sendHeader("X-Xclk", String(g_xclkMhz));
  server.sendHeader("X-Res", String(sizeName(g_size)) + " " + fb->width + "x" + fb->height);
  server.sendHeader("X-Comp", v(g_w.comp));
  server.sendHeader("X-Agc", v(g_w.agc));
  server.sendHeader("X-Gain", v(g_w.gain));
  server.sendHeader("X-Ae", v(g_w.ae));
  server.sendHeader("X-Exp", v(g_w.exp));
  server.sendHeader("X-Frame-Ms", String(frameMs));
  server.sendHeader("X-Grab-Ms", String(grabMs));
  server.sendHeader("X-Size-Ok", sizeOk ? "yes" : "NO");
  server.sendHeader("Cache-Control", "no-store");
  server.setContentLength(fb->len);
  server.send(200, "image/jpeg", "");
  server.client().write(fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  led(false);
  Serial.printf("step15 chip=%s psram_mb=%u\n", ESP.getChipModel(), (unsigned)(ESP.getPsramSize() >> 20));
  if (!startCamera()) return;
  WiFi.softAP("x4cam-exposure", "x4cam-steps");
  Serial.printf("wifi ap ssid=x4cam-exposure ip=%s\n", WiFi.softAPIP().toString().c_str());
  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.on("/shot", handleShot);
  server.begin();
  led(true);
}

void loop() { server.handleClient(); }
