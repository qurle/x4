// x4cam UI simulator: runs the real UI code (core/src/ui/ui.cpp, tokens, fonts, assets) with
// LVGL 9.6 on a PC. Only the device glue is faked: the clock, the display and touch, the status
// snapshot, and the app's request()/setOption(). Driver: tools/ui_sim/run.py. Guide: docs/DEVELOP.md.
//
//   ui_sim --list                                       scene table (name, figma frame, what)
//   ui_sim --scene <name> --photo <file> --out <bmp>     render one scene, headless
//   ui_sim --live --photo <file> [--scale N | --ppi N]   window you can click (needs SDL2);
//                                                        real screen size by default
//
// --photo is the "Photo Example" from Figma as raw big-endian RGB565, 320x240 (run.py makes it).
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "M5Unified.h"
#include "app/app.h"
#include "app/status.h"
#include "log/log_setup.h"
#include "lvgl.h"
#include "ui/ui.h"
#include "x4/log.h"

#ifdef X4_SIM_SDL
#include <SDL.h>
#endif

SimM5 M5;

namespace {
constexpr int kW = 320, kH = 240;
uint32_t g_ms = 0;
x4core::Status g_st{};
std::vector<uint8_t> g_photo;  // big-endian RGB565, kW * kH
uint16_t g_fb[kW * kH];        // what the panel would show (native RGB565)
struct {
  bool down = false;
  int16_t x = 0, y = 0;
} g_ptr;
bool g_live = false;

// Live window size. The CoreS3-SE screen is 2.0" at 320x240 (VERIFIED: M5 CoreS3-SE spec table),
// so 400 px on the diagonal = 200 px per inch and the picture is 1.6 x 1.2 in (40.6 x 30.5 mm,
// ASSUMPTION: the 2.0" is the visible area's diagonal). "Real size" = 1.6 in wide on your monitor.
constexpr float kDeviceWidthIn = 320.0f / 200.0f;
float g_scale = 0;  // --scale: window pixels per device pixel; 0 = real size
float g_ppi = 0;    // --ppi: your monitor's points (window units) per inch; 0 = ask SDL

// Fake shot / flash progress for the live mode.
struct {
  bool active = false;
  uint32_t startedAt = 0;
  bool withFlash = false;
} g_shot;
uint32_t g_flashFiredAt = 0;
constexpr uint32_t kFlashCoolMs = 8000;  // x4 default flash cooldown (shared/include/x4/safety.h)
}  // namespace

uint32_t millis() { return g_ms; }
uint32_t micros() { return g_ms * 1000u; }

// ------------------------------------------------------------------ fakes for the app side
namespace x4core {
namespace status {
Status get() { return g_st; }
}  // namespace status
namespace logsys {
Ring& ring() {
  static Ring r;
  return r;
}
}  // namespace logsys
namespace app {
void request(Request r) {
  switch (r) {
    case Request::FlashArm:
      g_st.flashArmed = true;
      break;
    case Request::FlashDisarm:
      g_st.flashArmed = false;
      break;
    case Request::Shutter:
      if (g_live && !g_shot.active && g_st.state == x4::shoot::State::Preview) {
        g_shot = {true, g_ms, g_st.flashArmed && g_st.flashReady};
        x4ui::shutterMoment(g_shot.withFlash);
      }
      break;
    default:
      break;
  }
  X4_LOGI("sim", "request", "r=%d", (int)r);
}
bool setOption(const char* name, int32_t v) {
  if (!strcmp(name, "ui_orient")) g_st.uiOrient = (uint8_t)v;
  else if (!strcmp(name, "ui_effect")) g_st.uiEffect = (uint8_t)v;
  else if (!strcmp(name, "cam_delay_ms")) g_st.camDelayMs = (uint16_t)v;
  else return false;
  X4_LOGI("sim", "set", "%s=%ld", name, (long)v);
  return true;
}
}  // namespace app
}  // namespace x4core

namespace {
using x4::shoot::State;

// ------------------------------------------------------------------ status presets
void baseStatus(x4core::Status& s) {
  s = x4core::Status{};
  s.state = State::Preview;
  s.previewOn = true;
  s.camsOnline = 4;
  for (int i = 0; i < 4; i++) {
    auto& c = s.cam[i];
    c.registered = c.online = true;
    snprintf(c.ip, sizeof(c.ip), "192.168.4.%d", 11 + i);
    snprintf(c.fw, sizeof(c.fw), "0.1.0");
    c.sensorPid = 0x039e;
    c.tempC = 45;
    c.rssi = -42;
    c.lastHbAgeMs = 400;
  }
  s.flashEnabled = true;
  s.flashReady = true;
  s.flashChargePct = 100;
  s.powerSafe = true;
  s.vbusMv = 5010;
  s.tempC = 41.5f;
  s.heap = 180000;
  s.heapMin = 150000;
  s.psram = 7000000;
  s.resetReason = "poweron";
  s.bootCount = 3;
  s.sdMounted = true;
  s.sdFreeMb = 820;
  s.sdTotalMb = 15000;
  s.fps = 30;
  s.previewFps = 5;
  s.apStations = 4;
}

void saving(x4core::Status& s, int stored) {
  s.state = State::Transferring;
  for (int i = 0; i < 4; i++) {
    s.cam[i].expected = true;
    s.cam[i].stored = i < stored;
  }
}

// ------------------------------------------------------------------ scenes
// figma = the frame in section 73:368 the scene should look like ("-" = not drawn in Figma).
struct Scene {
  const char* name;
  const char* figma;
  const char* what;
  void (*setup)(x4core::Status&);
  void (*after)();  // runs once the viewfinder is up (taps, screen changes)
};

void advance(uint32_t ms);
void tapScreen() {
  g_ptr = {true, 160, 140};
}
void openSettings() { x4ui::show(x4ui::Screen::Settings); }
void openDiag() { x4ui::show(x4ui::Screen::Diagnostics); }
// Portrait: a new camera frame must reach the faded side copies too (they share the buffer).
// The second frame is the first one inverted, so a stale side is easy to see.
void newFrame() {
  std::vector<uint8_t> inv(g_photo);
  for (auto& b : inv) b = (uint8_t)~b;
  x4ui::setPreviewJpeg(inv.data(), (uint32_t)inv.size());
}
// Portrait: a tap on a faded side must still reach the viewfinder (opens the menu).
void tapSide() { g_ptr = {true, 30, 140}; }
// A tap on the footer area is a tap on the photo too: the menu must open.
void tapFooter() { g_ptr = {true, 160, 225}; }

// Real touch on the orientation tile: exercises touch -> app::setOption -> status -> redraw.
void tapOrientTile() {
  openSettings();
  advance(600);
  g_ptr = {true, 86, 66};
}

const Scene kScenes[] = {
    {"preview_photo", "73:331", "viewfinder, footer default (SD free only)", [](x4core::Status&) {}, nullptr},
    {"preview_flash_recharge", "73:177", "flash armed and recharging: bolt + 80%",
     [](x4core::Status& s) {
       s.flashArmed = true;
       s.flashReady = false;
       s.flashChargePct = 80;
     },
     nullptr},
    {"preview_video", "73:236", "video mode, not recording: dot + -:--",
     [](x4core::Status& s) { s.videoMode = true; }, nullptr},
    {"preview_recording", "83:846", "recording 2:20 (Figma's mock text reads 2:2O)",
     [](x4core::Status& s) {
       s.videoMode = s.recording = true;
       s.recordMs = 140000;
     },
     nullptr},
    {"preview_dithered", "73:222", "style dithered (device dithers the live photo itself)",
     [](x4core::Status& s) { s.uiEffect = 1; }, nullptr},
    {"preview_portrait", "73:206", "orientation vertcl: centre crop + faded copies of the middle on the sides",
     [](x4core::Status& s) { s.uiOrient = 1; }, nullptr},
    {"portrait_new_frame", "-", "vertcl after a second (inverted) camera frame: the sides must follow",
     [](x4core::Status& s) { s.uiOrient = 1; }, newFrame},
    {"portrait_tap_side", "-", "vertcl, tap on the left side: the menu must open",
     [](x4core::Status& s) { s.uiOrient = 1; }, tapSide},
    {"tap_menu", "73:166", "tap menu (video and media hidden in v1)", [](x4core::Status&) {}, tapScreen},
    {"tap_footer", "-", "tap on the footer area: the menu must open", [](x4core::Status&) {}, tapFooter},
    {"settings", "73:254", "settings: horzntl, 0 s, normal, no flash", [](x4core::Status&) {}, openSettings},
    {"settings_b", "-", "settings: vertcl, 0.8 s, dithered, flash",
     [](x4core::Status& s) {
       s.uiOrient = 1;
       s.camDelayMs = 800;
       s.uiEffect = 1;
       s.flashArmed = true;
     },
     openSettings},
    {"settings_c", "-", "settings: cam delay 0.2 s", [](x4core::Status& s) { s.camDelayMs = 200; }, openSettings},
    {"settings_d", "-", "settings: cam delay 1 s", [](x4core::Status& s) { s.camDelayMs = 1000; }, openSettings},
    {"settings_tap", "-", "settings after tapping the orientation tile (touch path end to end)",
     [](x4core::Status&) {}, tapOrientTile},
    {"saving_1of4", "15:69", "Saving... 1 of 4", [](x4core::Status& s) { saving(s, 0); }, nullptr},
    {"saving_3of4", "15:80", "Saving... 3 of 4", [](x4core::Status& s) { saving(s, 2); }, nullptr},
    {"error_known", "83:2119", "error with a known reason: SD is full",
     [](x4core::Status& s) {
       s.state = State::Error;
       s.faults = (uint8_t)x4::shoot::Fault::SdFull;
       s.uiOrient = 1;
     },
     nullptr},
    {"error_unknown", "18:429", "error without a known reason",
     [](x4core::Status& s) {
       s.state = State::Error;
       s.faults = (uint8_t)x4::shoot::Fault::NoCameras;
       s.uiOrient = 1;
     },
     nullptr},
    {"diagnostics", "-", "diagnostics screen (not in Figma)", [](x4core::Status&) {}, openDiag},
};

// ------------------------------------------------------------------ LVGL glue
void flush(lv_display_t* d, const lv_area_t* a, uint8_t* px) {
  int32_t w = lv_area_get_width(a);
  for (int32_t y = a->y1; y <= a->y2; y++)
    memcpy(&g_fb[y * kW + a->x1], px + (size_t)(y - a->y1) * w * 2, (size_t)w * 2);
  lv_display_flush_ready(d);
}

void readPointer(lv_indev_t*, lv_indev_data_t* d) {
  d->point.x = g_ptr.x;
  d->point.y = g_ptr.y;
  d->state = g_ptr.down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

void logSink(const char* line, size_t len, x4::log::Level, void*) {
  x4core::logsys::ring().append(line, len);
  if (g_live) printf("%.*s\n", (int)len, line);
}

void initLvgl() {
  x4::log::setDevice("core");
  x4::log::setClock([]() -> uint32_t { return g_ms; });
  x4::log::addSink(logSink, nullptr, x4::log::Level::Debug);
  x4::log::setLevel(x4::log::Level::Debug);
  lv_init();
  lv_tick_set_cb([]() -> uint32_t { return g_ms; });
  lv_display_t* disp = lv_display_create(kW, kH);
  static uint8_t buf[kW * kH * 2];
  lv_display_set_buffers(disp, buf, nullptr, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(disp, flush);
  lv_indev_t* in = lv_indev_create();
  lv_indev_set_type(in, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(in, readPointer);
}

// Live mode only: a shot goes Shooting -> Transferring (one photo stored every 600 ms) -> Preview,
// and an armed flash recharges over 8 s afterwards. Rough timings, only for the feel.
void simTick() {
  if (g_shot.active) {
    uint32_t t = g_ms - g_shot.startedAt;
    if (t < 300) {
      g_st.state = State::Shooting;
    } else if (t < 300 + 4 * 600) {
      saving(g_st, (int)((t - 300) / 600));
    } else {
      g_shot.active = false;
      g_st.state = State::Preview;
      for (auto& c : g_st.cam) c.expected = c.stored = false;
      if (g_shot.withFlash) {
        g_st.flashReady = false;
        g_flashFiredAt = g_ms;
      }
      x4ui::shotStored(false, t, false);
    }
  }
  if (!g_st.flashReady && g_flashFiredAt) {
    uint32_t t = g_ms - g_flashFiredAt;
    g_st.flashChargePct = (uint8_t)(t >= kFlashCoolMs ? 100 : t * 100 / kFlashCoolMs);
    if (t >= kFlashCoolMs) g_st.flashReady = true;
  }
}

// Advance simulated time like the device loop: LVGL every 5 ms, the UI update every ~33 ms.
void advance(uint32_t ms) {
  for (uint32_t t = 0; t < ms; t += 5) {
    g_ms += 5;
    if (g_ms % 35 == 0) {
      simTick();
      x4ui::update(g_st, g_ms);
    }
    lv_timer_handler();
  }
}

void bootUi() {
  x4ui::begin(false);
  if (!g_photo.empty()) x4ui::setPreviewJpeg(g_photo.data(), (uint32_t)g_photo.size());
  advance(1800);  // splash (1.4 s) + fade to the viewfinder
}

bool writeBmp(const char* path) {
  FILE* f = fopen(path, "wb");
  if (!f) return false;
  const uint32_t row = kW * 3, size = 54 + row * kH;
  uint8_t h[54] = {'B', 'M'};
  auto put32 = [&](int at, uint32_t v) { memcpy(h + at, &v, 4); };
  put32(2, size);
  put32(10, 54);
  put32(14, 40);
  put32(18, kW);
  put32(22, (uint32_t)-kH);  // top-down rows
  h[26] = 1;
  h[28] = 24;
  put32(34, row * kH);
  fwrite(h, 1, 54, f);
  std::vector<uint8_t> line(row);
  for (int y = 0; y < kH; y++) {
    for (int x = 0; x < kW; x++) {
      uint16_t p = g_fb[y * kW + x];
      uint8_t r = (p >> 11) & 0x1F, g = (p >> 5) & 0x3F, b = p & 0x1F;
      line[x * 3 + 0] = (uint8_t)((b << 3) | (b >> 2));
      line[x * 3 + 1] = (uint8_t)((g << 2) | (g >> 4));
      line[x * 3 + 2] = (uint8_t)((r << 3) | (r >> 2));
    }
    fwrite(line.data(), 1, row, f);
  }
  fclose(f);
  return true;
}

bool loadPhoto(const char* path) {
  FILE* f = fopen(path, "rb");
  if (!f) return false;
  g_photo.resize(kW * kH * 2);
  size_t n = fread(g_photo.data(), 1, g_photo.size(), f);
  fclose(f);
  if (n != g_photo.size()) g_photo.clear();
  return !g_photo.empty();
}

int renderScene(const char* name, const char* out) {
  for (const Scene& sc : kScenes) {
    if (strcmp(sc.name, name)) continue;
    baseStatus(g_st);
    sc.setup(g_st);
    initLvgl();
    bootUi();
    if (sc.after) {
      sc.after();
      advance(80);
      g_ptr.down = false;  // ends a tap, if the scene started one
    }
    advance(900);  // let fades, slides and springs finish
    lv_refr_now(nullptr);
    if (!writeBmp(out)) {
      fprintf(stderr, "cannot write %s\n", out);
      return 1;
    }
    return 0;
  }
  fprintf(stderr, "unknown scene %s (try --list)\n", name);
  return 2;
}

#ifdef X4_SIM_SDL
void action(x4::input::Action a) {
  x4::input::Event e{};
  e.action = a;
  e.t = g_ms;
  x4ui::onAction(e);
}

int live() {
  g_live = true;
  baseStatus(g_st);
  initLvgl();
  bootUi();
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");  // smooth at non-integer sizes, like a real panel
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    return 1;
  }
  SDL_Window* win = SDL_CreateWindow("x4cam UI sim", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, kW, kH,
                                     SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);
  SDL_Renderer* ren = SDL_CreateRenderer(win, -1, 0);
  SDL_Texture* tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, kW, kH);
  SDL_RenderSetLogicalSize(ren, kW, kH);  // keeps 4:3 and maps the mouse at any window size

  // Window size in window units (points on macOS). Real size needs the monitor's points per inch:
  // --ppi, else SDL's estimate. SDL reports DPI in pixels; on a HiDPI/Retina screen a point is
  // several pixels, so divide by that ratio (Not sure SDL's number is right on every system:
  // measure the window and pass --ppi if it isn't 40.6 mm wide).
  int ww = 0, wh = 0, pw = 0, ph = 0;
  SDL_GetWindowSize(win, &ww, &wh);
  SDL_GetRendererOutputSize(ren, &pw, &ph);
  float pixelsPerPoint = ww > 0 && pw > 0 ? (float)pw / ww : 1.0f;
  float ppi = g_ppi;
  const char* ppiFrom = "--ppi";
  if (ppi <= 0) {
    float hdpi = 0;
    if (SDL_GetDisplayDPI(0, nullptr, &hdpi, nullptr) == 0 && hdpi > 40 && hdpi < 1200) {
      ppi = hdpi / pixelsPerPoint;
      ppiFrom = "SDL";
    }
  }
  float realW = ppi > 0 ? kDeviceWidthIn * ppi : 0;
  auto setWidth = [&](float w) {
    if (w < 160) w = 160;
    SDL_SetWindowSize(win, (int)(w + 0.5f), (int)(w * kH / kW + 0.5f));
    SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
  };
  float startW = g_scale > 0 ? kW * g_scale : realW > 0 ? realW : kW;
  setWidth(startW);
  if (g_scale > 0)
    printf("window: %.0f x %.0f (--scale %.2f)\n", startW, startW * kH / kW, g_scale);
  else if (realW > 0)
    printf("window: %.0f x %.0f = real size, 40.6 x 30.5 mm at %.0f points/inch (from %s).\n"
           "  Check with a ruler. If the width isn't 40.6 mm, run again with --ppi %.0f x 40.6 / <measured mm>.\n",
           realW, realW * kH / kW, ppi, ppiFrom, ppi);
  else
    printf("window: 320 x 240 (monitor size unknown: pass --ppi for real size)\n");
  printf(
      "x4cam UI sim. Mouse = touch. Keys:\n"
      "  space shutter   f flash arm/disarm   e cycle errors   c cycle SD (820 MB / 12.3 GB / none)\n"
      "  v video mode    r recording          p preview on/off\n"
      "  m menu action   d diagnostics        esc back         s screenshot   q quit\n"
      "  + / - bigger / smaller   0 back to the start size (or drag the window corner)\n");
  int errorStep = 0, sdStep = 0, shots = 0;
  uint32_t last = SDL_GetTicks();
  bool run = true;
  while (run) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_QUIT) run = false;
      if (ev.type == SDL_MOUSEBUTTONDOWN || ev.type == SDL_MOUSEBUTTONUP || ev.type == SDL_MOUSEMOTION) {
        int x = 0, y = 0;
        Uint32 b = SDL_GetMouseState(&x, &y);
        float lx = 0, ly = 0;
        SDL_RenderWindowToLogical(ren, x, y, &lx, &ly);
        g_ptr = {(b & SDL_BUTTON_LMASK) != 0, (int16_t)lx, (int16_t)ly};
      }
      if (ev.type != SDL_KEYDOWN) continue;
      switch (ev.key.keysym.sym) {
        case SDLK_q: run = false; break;
        case SDLK_EQUALS:
        case SDLK_PLUS:
        case SDLK_KP_PLUS:
        case SDLK_MINUS:
        case SDLK_KP_MINUS: {
          bool up = ev.key.keysym.sym != SDLK_MINUS && ev.key.keysym.sym != SDLK_KP_MINUS;
          SDL_GetWindowSize(win, &ww, &wh);
          setWidth(ww * (up ? 1.1f : 1 / 1.1f));
          SDL_GetWindowSize(win, &ww, &wh);
          printf("window: %d x %d (%.2fx real size)\n", ww, wh, realW > 0 ? ww / realW : 0.0f);
          break;
        }
        case SDLK_0: setWidth(startW); break;
        case SDLK_SPACE: x4core::app::request(x4core::app::Request::Shutter); break;
        case SDLK_f:
          x4core::app::request(g_st.flashArmed ? x4core::app::Request::FlashDisarm
                                               : x4core::app::Request::FlashArm);
          break;
        case SDLK_e: {
          static const uint8_t kFaults[] = {0, (uint8_t)x4::shoot::Fault::SdFull, (uint8_t)x4::shoot::Fault::SdMissing,
                                            (uint8_t)x4::shoot::Fault::Overheat, (uint8_t)x4::shoot::Fault::NoCameras};
          errorStep = (errorStep + 1) % 5;
          g_st.faults = kFaults[errorStep];
          g_st.state = errorStep ? State::Error : State::Preview;
          break;
        }
        case SDLK_c:
          sdStep = (sdStep + 1) % 3;
          g_st.sdMounted = sdStep != 2;
          g_st.sdFreeMb = sdStep == 0 ? 820 : 12600;
          break;
        case SDLK_v: g_st.videoMode = !g_st.videoMode; break;
        case SDLK_r:
          g_st.recording = !g_st.recording;
          if (g_st.recording) g_st.videoMode = true;
          break;
        case SDLK_p: g_st.previewOn = !g_st.previewOn; break;
        case SDLK_m: action(x4::input::Action::Menu); break;
        case SDLK_d: action(x4::input::Action::Diagnostics); break;
        case SDLK_ESCAPE: action(x4::input::Action::Back); break;
        case SDLK_s: {
          char path[48];
          snprintf(path, sizeof(path), "ui_sim_%02d.bmp", ++shots);
          if (writeBmp(path)) printf("saved %s\n", path);
          break;
        }
        default: break;
      }
    }
    uint32_t now = SDL_GetTicks();
    if (g_st.recording) g_st.recordMs += now - last;
    advance(now - last < 100 ? now - last : 100);
    last = now;
    SDL_UpdateTexture(tex, nullptr, g_fb, kW * 2);
    SDL_RenderClear(ren);
    SDL_RenderCopy(ren, tex, nullptr, nullptr);
    SDL_RenderPresent(ren);
    SDL_Delay(5);
  }
  SDL_Quit();
  return 0;
}
#endif
}  // namespace

int main(int argc, char** argv) {
  setvbuf(stdout, nullptr, _IOLBF, 0);  // log lines show up at once, even through a pipe
  const char *scene = nullptr, *out = nullptr, *photo = nullptr;
  bool list = false, liveMode = false;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--list")) list = true;
    else if (!strcmp(argv[i], "--live")) liveMode = true;
    else if (!strcmp(argv[i], "--scene") && i + 1 < argc) scene = argv[++i];
    else if (!strcmp(argv[i], "--out") && i + 1 < argc) out = argv[++i];
    else if (!strcmp(argv[i], "--photo") && i + 1 < argc) photo = argv[++i];
    else if (!strcmp(argv[i], "--scale") && i + 1 < argc) g_scale = (float)atof(argv[++i]);
    else if (!strcmp(argv[i], "--ppi") && i + 1 < argc) g_ppi = (float)atof(argv[++i]);
  }
  if (list) {
    for (const Scene& s : kScenes) printf("%s\t%s\t%s\n", s.name, s.figma, s.what);
    return 0;
  }
  if (photo && !loadPhoto(photo)) fprintf(stderr, "photo %s unreadable: viewfinder stays empty\n", photo);
  if (liveMode) {
#ifdef X4_SIM_SDL
    return live();
#else
    fprintf(stderr, "built without SDL2: install it (see docs/DEVELOP.md) and rebuild for --live\n");
    return 2;
#endif
  }
  if (!scene || !out) {
    fprintf(stderr, "usage: ui_sim --list | --scene <name> --photo <file> --out <file.bmp> | --live --photo <file> [--scale N | --ppi N]\n");
    return 2;
  }
  return renderScene(scene, out);
}
