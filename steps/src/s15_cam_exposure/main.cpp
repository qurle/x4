// Step 15: exposure test on one camera. Can the camera freeze motion indoors?
// The camera module runs its own auto exposure; in dim light it picks long exposures and
// everything that moves blurs. The esp32-camera driver has only three exposure controls for this
// module (VERIFIED, sensors/mega_ccm.c): AGC/AE mode (reg 0x0130), manual gain (0x0131) and
// manual exposure (0x0133 high byte, 0x0134 low byte). Its exposure setter writes the HIGH byte
// into BOTH registers (bug, VERIFIED in the source), so this firmware writes the registers
// directly with the driver's own SCCB_Write16 (exported by the pinned library).
// What the values MEAN (units, ranges, which mode value is manual) is Not sure: M5 doesn't
// document them. Finding that out is the point of this step.
//
// Wiring: camera on M5's Grove-to-USB-C adapter only, like step 8.
// Use: join WiFi "x4cam-exposure" (password x4cam-steps), open http://192.168.4.1/
//      1. "Read registers" in auto mode: see what the camera picked by itself.
//      2. Set mode / exposure / gain, "Apply", then "Shoot" a moving thing (fan, waving hand).
// Pass (the experiment works): some setting makes motion sharp, and the frame time or brightness
//      changes when you change exposure. Write the numbers into MEASUREMENTS "Exposure".
// If the camera stops answering after a write: unplug and replug it (all settings reset).
#include <WebServer.h>
#include <WiFi.h>

#include "cam_common.h"

// From the esp32-camera driver (private header sccb.h, functions exported by the library).
extern "C" uint8_t SCCB_Read16(uint8_t slv_addr, uint16_t reg);
extern "C" int SCCB_Write16(uint8_t slv_addr, uint16_t reg, uint8_t data);

constexpr uint8_t kSccbAddr = 0x1F;  // MEGA_CCM SCCB address (cam_common.h, VERIFIED)
constexpr uint16_t kRegAeMode = 0x0130;
constexpr uint16_t kRegGain = 0x0131;
constexpr uint16_t kRegExpH = 0x0133;
constexpr uint16_t kRegExpL = 0x0134;
constexpr uint16_t kRegExpComp = 0x0125;  // "EXP_COMPENSATE" in the driver's register list (unused by it)

WebServer server(80);
uint32_t g_shots = 0;

struct Readback {
  uint8_t mode, gain, expH, expL, comp, res, fwVer;
};

Readback readRegs() {
  Readback r;
  r.mode = SCCB_Read16(kSccbAddr, kRegAeMode);
  r.gain = SCCB_Read16(kSccbAddr, kRegGain);
  r.expH = SCCB_Read16(kSccbAddr, kRegExpH);
  r.expL = SCCB_Read16(kSccbAddr, kRegExpL);
  r.comp = SCCB_Read16(kSccbAddr, kRegExpComp);
  r.res = SCCB_Read16(kSccbAddr, 0x0121);   // RESOLUTION_REG
  r.fwVer = SCCB_Read16(kSccbAddr, 0x0002); // FIRMWARE_VER
  return r;
}

String regsJson(const Readback& r) {
  char b[200];
  snprintf(b, sizeof(b),
           "{\"mode\":%u,\"gain\":%u,\"exp\":%u,\"exp_h\":%u,\"exp_l\":%u,\"comp\":%u,\"res_reg\":%u,\"fw\":%u}",
           r.mode, r.gain, (unsigned)((r.expH << 8) | r.expL), r.expH, r.expL, r.comp, r.res, r.fwVer);
  return String(b);
}

framesize_t sizeFor(const String& s) {
  if (s == "vga") return FRAMESIZE_VGA;
  if (s == "hd") return FRAMESIZE_HD;
  if (s == "uxga") return FRAMESIZE_UXGA;
  return FRAMESIZE_5MP;
}

// Time between consecutive frames. If a long manual exposure is honoured, this usually grows
// (the frame can't be shorter than the exposure). ASSUMPTION for this module: test it.
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

void handleRoot() {
  static const char kPage[] PROGMEM = R"HTML(<!doctype html><html><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1"><title>x4cam step 15</title>
<style>body{font:15px system-ui;margin:12px;background:#111;color:#eee}input,select,button{font:inherit;margin:2px}
input[type=number]{width:6em}img{max-width:100%;margin-top:8px}table{border-collapse:collapse;font:12px monospace}
td,th{border:1px solid #444;padding:2px 5px}pre{white-space:pre-wrap}</style></head><body>
<h2>x4cam step 15: exposure</h2>
<p>mode (0x0130) <input id=mode type=number min=0 max=255 value=1>
 gain (0x0131) <input id=gain type=number min=0 max=255 value=0>
 exposure (0x0133/34) <input id=exp type=number min=0 max=65535 value=0>
 comp (0x0125) <input id=comp type=number min=0 max=255 value=0></p>
<p>size <select id=res><option value=vga>VGA</option><option value=hd>HD</option><option value=uxga>UXGA</option><option value=5mp selected>5 MP</option></select>
 <label><input id=wmode type=checkbox checked>write mode</label>
 <label><input id=wgain type=checkbox>write gain</label>
 <label><input id=wexp type=checkbox>write exposure</label>
 <label><input id=wcomp type=checkbox>write comp</label></p>
<p><button onclick=apply()>Apply</button> <button onclick=regs()>Read registers</button>
 <button onclick=shoot()>Shoot</button></p>
<pre id=out></pre>
<table id=log><tr><th>#</th><th>size</th><th>mode</th><th>gain</th><th>exp</th><th>comp</th><th>frame ms</th><th>grab ms</th><th>kB</th></tr></table>
<img id=img>
<script>
const $=id=>document.getElementById(id);
function q(){let p=new URLSearchParams({res:$('res').value});
 for(const k of ['mode','gain','exp','comp']) if($('w'+k).checked) p.set(k,$(k).value); return p}
async function apply(){const r=await fetch('/set?'+q());$('out').textContent='applied, readback: '+await r.text()}
async function regs(){const r=await fetch('/regs');$('out').textContent='registers: '+await r.text()}
async function shoot(){$('out').textContent='shooting...';
 const r=await fetch('/shot');if(!r.ok){$('out').textContent='shot failed';return}
 const h=k=>r.headers.get('X-'+k);const b=await r.blob();$('img').src=URL.createObjectURL(b);
 const row=$('log').insertRow(1);
 [h('Shot'),h('Res'),h('Mode'),h('Gain'),h('Exp'),h('Comp'),h('Frame-Ms'),h('Grab-Ms'),(b.size/1024).toFixed(0)].forEach(v=>row.insertCell().textContent=v);
 $('out').textContent='ok'}
</script></body></html>)HTML";
  server.send_P(200, "text/html", kPage);
}

void handleRegs() {
  Readback r = readRegs();
  String j = regsJson(r);
  Serial.printf("exp regs %s\n", j.c_str());
  server.send(200, "application/json", j);
}

void handleSet() {
  sensor_t* s = esp_camera_sensor_get();
  if (server.hasArg("res") && s) s->set_framesize(s, sizeFor(server.arg("res")));
  int err = 0;
  // Mode first: on many modules the manual values only stick once auto is off (Not sure here).
  // The driver calls this register "AGC mode" and writes 1 to enable it, so 1 = auto and
  // 0 = manual is the likely meaning (ASSUMPTION from the driver's naming).
  if (server.hasArg("mode")) err += SCCB_Write16(kSccbAddr, kRegAeMode, (uint8_t)server.arg("mode").toInt());
  if (server.hasArg("gain")) err += SCCB_Write16(kSccbAddr, kRegGain, (uint8_t)server.arg("gain").toInt());
  if (server.hasArg("exp")) {
    uint16_t e = (uint16_t)server.arg("exp").toInt();
    err += SCCB_Write16(kSccbAddr, kRegExpH, e >> 8);
    err += SCCB_Write16(kSccbAddr, kRegExpL, e & 0xFF);
  }
  if (server.hasArg("comp")) err += SCCB_Write16(kSccbAddr, kRegExpComp, (uint8_t)server.arg("comp").toInt());
  cameraWarmUp(2);  // new settings show up a frame or two later
  Readback r = readRegs();
  String j = regsJson(r);
  String args;
  for (int i = 0; i < server.args(); i++) args += server.argName(i) + "=" + server.arg(i) + " ";
  Serial.printf("exp set %swrite_err=%d readback=%s\n", args.c_str(), err, j.c_str());
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
  Readback r = readRegs();
  g_shots++;
  const char* res = fb->width >= 2592 ? "5mp" : fb->width >= 1600 ? "uxga" : fb->width >= 1280 ? "hd" : "vga";
  Serial.printf("exp shot n=%lu res=%s w=%u h=%u mode=%u gain=%u exp=%u comp=%u frame_ms=%lu grab_ms=%lu bytes=%u\n",
                (unsigned long)g_shots, res, fb->width, fb->height, r.mode, r.gain, (unsigned)((r.expH << 8) | r.expL),
                r.comp, (unsigned long)frameMs, (unsigned long)grabMs, (unsigned)fb->len);
  server.sendHeader("X-Shot", String(g_shots));
  server.sendHeader("X-Res", res);
  server.sendHeader("X-Mode", String(r.mode));
  server.sendHeader("X-Gain", String(r.gain));
  server.sendHeader("X-Exp", String((r.expH << 8) | r.expL));
  server.sendHeader("X-Comp", String(r.comp));
  server.sendHeader("X-Frame-Ms", String(frameMs));
  server.sendHeader("X-Grab-Ms", String(grabMs));
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
  if (cameraBegin(FRAMESIZE_5MP) == 0) return;
  cameraWarmUp();
  Serial.printf("exp boot regs %s\n", regsJson(readRegs()).c_str());
  WiFi.softAP("x4cam-exposure", "x4cam-steps");
  Serial.printf("wifi ap ssid=x4cam-exposure ip=%s\n", WiFi.softAPIP().toString().c_str());
  server.on("/", handleRoot);
  server.on("/regs", handleRegs);
  server.on("/set", handleSet);
  server.on("/shot", handleShot);
  server.begin();
  led(true);
}

void loop() { server.handleClient(); }
