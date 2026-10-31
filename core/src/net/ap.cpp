#include <WiFi.h>
#include <esp_wifi.h>

#include "net.h"
#include "x4/log.h"
#include "x4/safety.h"
#include "x4_secrets.h"

namespace x4core {
namespace net {

bool beginAp(const x4::cfg::Config& c) {
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  // 192.168.4.1/24 is the ESP32 SoftAP default; set explicitly so cameras can hard-code it.
  IPAddress ip(192, 168, 4, 1), mask(255, 255, 255, 0);
  WiFi.softAPConfig(ip, ip, mask);
  int maxConn = c.ap_max_clients;
  if (maxConn < x4::safety::kApMinClients) maxConn = x4::safety::kApMinClients;
  if (maxConn > x4::safety::kApMaxClients) maxConn = x4::safety::kApMaxClients;
  // Arduino's softAP() default max_connection is 4: too few for 4 cameras + a phone.
  bool ok = WiFi.softAP(X4_AP_SSID, X4_AP_PASS, c.ap_channel, c.ap_hidden, maxConn);
  if (!ok) {
    X4_LOG_ERR(Error, "net", "ap", x4::Err::NET_AP_FAIL, "ssid=%s ch=%ld", X4_AP_SSID, (long)c.ap_channel);
    return false;
  }
  esp_wifi_set_ps(WIFI_PS_NONE);  // power save adds 100+ ms random latency
  esp_wifi_set_max_tx_power((int8_t)c.wifi_tx_qdbm);
  wifi_config_t conf;
  esp_wifi_get_config(WIFI_IF_AP, &conf);
  int8_t txp = 0;
  esp_wifi_get_max_tx_power(&txp);
  X4_LOGI("net", "ap", "ssid=%s ip=%s ch=%ld max_conn=%u (verified=%u) hidden=%ld tx_qdbm=%d default_pass=%d",
          X4_AP_SSID, WiFi.softAPIP().toString().c_str(), (long)c.ap_channel, (unsigned)maxConn,
          (unsigned)conf.ap.max_connection, (long)c.ap_hidden, (int)txp, X4_SECRETS_ARE_DEFAULT);
  return true;
}

int apStations() { return WiFi.softAPgetStationNum(); }

}  // namespace net
}  // namespace x4core
