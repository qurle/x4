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
// Round 2 (owner's first results, 2026-11-15: manual exposure needs AGC mode = 1; auto was brighter
// than the highest manual exposure tried; 5 MP frame 207 ms vs VGA 40 ms): QVGA, a brightness
// number per shot (luma), fps, exposure/gain sweeps and a preview-size-then-5-MP test, to tune
// for a 24+ fps preview and a short exposure on the shot.
//
// Wiring: camera on M5's Grove-to-USB-C adapter only, like step 8.
// Use: join WiFi "x4cam-exposure" (password x4cam-steps), open http://192.168.4.1/
// If the camera stops answering: unplug and replug it (all settings reset).
#include <WebServer.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <jpeg_decoder.h>  // esp_jpeg (TJpgDec), part of the Arduino-ESP32 libs

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
    case FRAMESIZE_QVGA: return "qvga";
    case FRAMESIZE_VGA: return "vga";
    case FRAMESIZE_HD: return "hd";
    case FRAMESIZE_UXGA: return "uxga";
    case FRAMESIZE_FHD: return "fhd";
    default: return "5mp";
  }
}

framesize_t sizeFor(const String& s) {
  if (s == "qvga") return FRAMESIZE_QVGA;
  if (s == "vga") return FRAMESIZE_VGA;
  if (s == "hd") return FRAMESIZE_HD;
  if (s == "uxga") return FRAMESIZE_UXGA;
  if (s == "fhd") return FRAMESIZE_FHD;
  return FRAMESIZE_5MP;
}

int expectedWidth(framesize_t f) {
  switch (f) {
    case FRAMESIZE_QVGA: return 320;
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

// Time between consecutive frames (1000 / frame_ms = the fps a preview stream could get from this
// camera at this size, before WiFi and the Core's decoding). Frames can't be shorter than the
// exposure, so if a long manual exposure is honoured this usually grows (ASSUMPTION for this
// module: test it). Averages at least 4 frames and 600 ms, at most 16 frames.
uint32_t framePeriodMs() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (fb) esp_camera_fb_return(fb);
  uint32_t t0 = millis();
  int got = 0;
  while (got < 16 && (got < 4 || millis() - t0 < 600)) {
    fb = esp_camera_fb_get();
    if (!fb) break;
    esp_camera_fb_return(fb);
    got++;
  }
  return got ? (millis() - t0) / got : 0;
}

// Average brightness of a JPEG, 0 (black) .. 255 (white), from a 1/8-scale decode (a 5 MP photo
// becomes 324x243). (R + 2G + B) / 4: close to real luma, and the same whichever way round the
// decoder orders R and B. -1 = decode failed. Takes a few hundred ms at 5 MP (TJpgDec still
// parses the whole file).
int meanLuma(const uint8_t* jpg, size_t len) {
  esp_jpeg_image_cfg_t cfg = {};
  cfg.indata = (uint8_t*)jpg;
  cfg.indata_size = len;
  cfg.out_format = JPEG_IMAGE_FORMAT_RGB888;
  cfg.out_scale = JPEG_IMAGE_SCALE_1_8;
  esp_jpeg_image_output_t info = {};
  if (esp_jpeg_get_image_info(&cfg, &info) != ESP_OK || info.output_len == 0) return -1;
  uint8_t* rgb = (uint8_t*)heap_caps_malloc(info.output_len, MALLOC_CAP_SPIRAM);
  if (!rgb) return -1;
  cfg = {};
  cfg.indata = (uint8_t*)jpg;
  cfg.indata_size = len;
  cfg.outbuf = rgb;
  cfg.outbuf_size = info.output_len;
  cfg.out_format = JPEG_IMAGE_FORMAT_RGB888;
  cfg.out_scale = JPEG_IMAGE_SCALE_1_8;
  esp_jpeg_image_output_t out = {};
  int luma = -1;
  if (esp_jpeg_decode(&cfg, &out) == ESP_OK) {
    uint64_t sum = 0;
    size_t px = out.output_len / 3;
    for (size_t i = 0; i < px; i++) sum += rgb[3 * i] + 2u * rgb[3 * i + 1] + rgb[3 * i + 2];
    if (px) luma = (int)(sum / (4 * px));
  }
  heap_caps_free(rgb);
  return luma;
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
input[type=number]{width:6em}img{display:block;max-width:100%;margin:8px 0}table{border-collapse:collapse;font:12px monospace}
td,th{border:1px solid #444;padding:2px 5px}pre{white-space:pre-wrap}fieldset{border:1px solid #444;margin:8px 0}
.mu{color:#999;font-size:13px}</style></head><body>
<h2>x4cam step 15: exposure</h2>
<p class=mu>Only ticked fields are written. Nothing is read back from the camera (reads while it
streams break it), so "written" shows what we sent. Just after boot a write can be ignored: wait a
minute and Apply again.</p>
<fieldset><legend>1. exposure compensation (0x0125): little effect so far</legend>
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
size <select id=res><option value=qvga>QVGA (preview)</option><option value=vga selected>VGA</option><option value=hd>HD</option><option value=uxga>UXGA</option>
<option value=fhd>FHD</option><option value=5mp>5 MP</option></select>
clock <select id=xclk><option value=10>10 MHz</option><option value=16>16 MHz</option><option value=20 selected>20 MHz</option></select>
<span class=mu>(a new clock restarts the camera: all register settings reset)</span>
</fieldset>
<p><button onclick="run(apply)">Apply</button> <button onclick="run(()=>shoot(''))">Shoot</button></p>
<fieldset><legend>4. sweep (manual mode)</legend>
<select id=swk><option value=exp>exposure</option><option value=gain>gain</option></select>
values <input id=swv size=34 value="25 50 100 200 400 800 1600 3200">
<button onclick="run(sweep)">Sweep</button><br>
<span class=mu>Writes AGC mode = 1 and each value in turn (plus the other ticked fields), one shot each.
Sweeping gain: tick exposure too, so it stays fixed.</span>
</fieldset>
<fieldset><legend>5. preview vs shot</legend>
<button onclick="run(pair)">Preview size, then 5 MP</button><br>
<span class=mu>With the ticked fields: a shot at the size picked above, a 5 MP shot without rewriting
anything (did the setting survive the size switch?), a 5 MP shot after rewriting (same brightness as
the preview? then the exposure number is time, not sensor lines), then back.</span>
</fieldset>
<pre id=out></pre>
<img id=img>
<p class=mu>luma = average brightness of the photo, 0 (black) to 255 (white). fps = 1000 / frame ms:
what a preview stream could get from the camera, before WiFi and the Core. Click a row to see its photo.</p>
<table id=log><tr><th>#</th><th>MHz</th><th>size</th><th>comp</th><th>agc</th><th>gain</th><th>ae</th><th>exp</th><th>frame ms</th><th>fps</th><th>grab ms</th><th>luma</th><th>kB</th><th>size ok</th><th>note</th></tr></table>
<script>
const $=id=>document.getElementById(id);
const say=t=>$('out').textContent=t;
let busy=false;
async function run(f){if(busy)return;busy=true;try{await f()}catch(e){say(String(e.message||e))}busy=false}
function base(res){return new URLSearchParams({res:res||$('res').value,xclk:$('xclk').value})}
function ticked(p,skip){for(const k of ['comp','agc','gain','ae','exp']) if(k!==skip&&$('w'+k).checked) p.set(k,$(k).value); return p}
async function put(p){say('applying...');const r=await fetch('/set?'+p);const t=await r.text();
 if(!r.ok)throw new Error('write error: '+t);say('written: '+t)}
const apply=()=>put(ticked(base()));
async function shoot(note){say('shooting...');
 const r=await fetch('/shot');if(!r.ok)throw new Error('shot failed: '+await r.text());
 const h=k=>r.headers.get('X-'+k);const b=await r.blob();const url=URL.createObjectURL(b);$('img').src=url;
 const row=$('log').insertRow(1);row.onclick=()=>$('img').src=url;row.style.cursor='pointer';
 const fm=+h('Frame-Ms');
 [h('Shot'),h('Xclk'),h('Res'),h('Comp'),h('Agc'),h('Gain'),h('Ae'),h('Exp'),fm,fm?(1000/fm).toFixed(1):'-',h('Grab-Ms'),h('Luma'),(b.size/1024).toFixed(0),h('Size-Ok'),note].forEach(v=>row.insertCell().textContent=v);
 say(h('Size-Ok')=='yes'?'ok':'camera fell back to a different size: Apply again, or replug')}
async function sweep(){const k=$('swk').value;const vals=$('swv').value.split(/[\s,]+/).filter(Boolean);
 for(const v of vals){const p=ticked(base(),k);p.set('agc','1');p.set(k,v);await put(p);await shoot('sweep '+k+'='+v)}
 say('sweep done')}
async function pair(){const pv=$('res').value;if(pv=='5mp')throw new Error('pick the preview size (QVGA or VGA) first');
 await put(ticked(base()));await shoot('pair: '+pv);
 await put(base('5mp'));await shoot('pair: 5mp, not rewritten');
 await put(ticked(base('5mp')));await shoot('pair: 5mp, rewritten');
 await put(ticked(base()));say('pair done, back at '+pv)}
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
  int luma = meanLuma(fb->buf, fb->len);
  g_shots++;
  Serial.printf("exp shot n=%lu xclk_mhz=%d size=%s w=%u h=%u size_ok=%d comp=%d agc=%d gain=%d ae=%d exp=%ld "
                "frame_ms=%lu grab_ms=%lu luma=%d bytes=%u\n",
                (unsigned long)g_shots, g_xclkMhz, sizeName(g_size), fb->width, fb->height, sizeOk, g_w.comp, g_w.agc,
                g_w.gain, g_w.ae, g_w.exp, (unsigned long)frameMs, (unsigned long)grabMs, luma, (unsigned)fb->len);
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
  server.sendHeader("X-Luma", luma < 0 ? String("?") : String(luma));
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
