// Camera bring-up for the Unit CamS3-5MP. Pins: camera/include/cam_pins.h (VERIFIED, M5 docs
// + schematic). Sensor MEGA_CCM at SCCB 0x1F, expected PID 0x039E.
#pragma once
#include <Arduino.h>
#include <esp_camera.h>

constexpr uint16_t kExpectedPid = 0x039E;
constexpr int kLedPin = 14;  // blue LED, LOW = on

inline void led(bool on) {
  pinMode(kLedPin, OUTPUT);
  digitalWrite(kLedPin, on ? LOW : HIGH);
}

// JPEG straight from the sensor, frame buffers in PSRAM. Returns the sensor PID (0 = failed).
inline uint16_t cameraBegin(framesize_t size) {
  camera_config_t c = {};
  c.pin_pwdn = -1;    // tied to GND on the board
  c.pin_reset = 21;
  c.pin_xclk = 11;
  c.pin_sccb_sda = 17;
  c.pin_sccb_scl = 41;
  c.pin_d7 = 13; c.pin_d6 = 4; c.pin_d5 = 10; c.pin_d4 = 5;
  c.pin_d3 = 7;  c.pin_d2 = 16; c.pin_d1 = 15; c.pin_d0 = 6;
  c.pin_vsync = 42;
  c.pin_href = 18;
  c.pin_pclk = 12;
  c.xclk_freq_hz = 20000000;
  c.ledc_timer = LEDC_TIMER_0;
  c.ledc_channel = LEDC_CHANNEL_0;
  c.pixel_format = PIXFORMAT_JPEG;
  c.frame_size = size;
  c.jpeg_quality = 12;  // lower = better quality, bigger file
  c.fb_count = 2;
  c.fb_location = CAMERA_FB_IN_PSRAM;
  c.grab_mode = CAMERA_GRAB_LATEST;
  c.jpeg_buffer_size = 2560 * 1024;  // a 5 MP JPEG at quality 12 fits (same as cam_jpeg_buf_kb)
  esp_err_t err = esp_camera_init(&c);
  if (err != ESP_OK) {
    Serial.printf("cam init FAILED esp_err=0x%x (psram_mb=%u)\n", (unsigned)err,
                  (unsigned)(ESP.getPsramSize() >> 20));
    return 0;
  }
  sensor_t* s = esp_camera_sensor_get();
  uint16_t pid = s ? s->id.PID : 0;
  Serial.printf("cam sensor pid=0x%04x expected=0x%04x ok=%d\n", pid, kExpectedPid, pid == kExpectedPid);
  return pid;
}

// Grab a frame and throw it away (the first frames after start or a size change can be bad).
inline void cameraWarmUp(int frames = 2) {
  for (int i = 0; i < frames; i++) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (fb) esp_camera_fb_return(fb);
  }
}
