// Network names for steps 9-11. The Core makes the WiFi; cameras join it.
// Bench defaults on purpose: the real firmware uses core/include/secrets.h instead.
#pragma once
#include <IPAddress.h>

#define STEPS_AP_SSID "x4cam-steps"
#define STEPS_AP_PASS "x4cam-steps"  // WPA2 needs >= 8 characters

// Fixed camera addresses, so the Core knows who is who without any registration protocol:
// camera N is 192.168.4.(10+N). The Core is 192.168.4.1 (the SoftAP default).
inline IPAddress cameraIp(int id) { return IPAddress(192, 168, 4, 10 + id); }
constexpr int kMaxCameras = 4;
