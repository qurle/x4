# Companion app

Not on the critical path. Shooting works with no phone, no router and no internet. The companion
mostly does **post-processing**. Plan: **web debug page first (served by the Core), Flutter later
(Android + iOS, both required)**. Do not start Flutter until the owner asks.

## 1. Web debug/viewer page (done, Stage 2)

Served by the Core at `http://192.168.4.1/` from `core/src/net/web_page.h`.
Same origin as the API → no CORS preflights, no mixed-content blocking. A page hosted on HTTPS on
the internet **cannot** talk to the rig (mixed content + the AP has no internet).
Features: device status, cameras, live logs, config editor, latest photos, manual trigger,
bring-up actions. Contract: [PROTOCOL.md](PROTOCOL.md#core-http-api-port-80).

## 2. Flutter app (later)

- **Join the AP programmatically.**
  - Android 10+: `WifiNetworkSpecifier` via `ConnectivityManager.requestNetwork`; then bind the
    process to that network (`bindProcessToNetwork`) or the OS keeps routing traffic over mobile
    data because the AP has no internet.
  - iOS: `NEHotspotConfiguration` (needs the **Hotspot Configuration** entitlement in the app ID
    and provisioning profile). iOS may show "no internet" and keep cellular for other traffic.
- **Cleartext HTTP to 192.168.4.1** must be allowed:
  - Android: `network_security_config.xml` with `<domain-config cleartextTrafficPermitted="true"><domain includeSubdomains="false">192.168.4.1</domain></domain-config>`.
  - iOS: `NSAppTransportSecurity` → `NSAllowsLocalNetworking = YES` (or an exception domain), plus
    `NSLocalNetworkUsageDescription` for the local-network permission prompt (iOS 14+).
- API: `GET /api/shots`, `GET /file?path=…`, `GET /api/status`, `POST /api/action?a=shutter`.
  App-agnostic, versioned with `proto`.
- Post-processing candidates: alignment of the four views, wiggle GIF/stereo, ordered (Bayer)
  dithering to 1-bit / 16-level BMP (reference implementation: `shared/src/dither.cpp`, tested),
  batch export.

### Android: staying on your own WiFi (investigated 2026-11-13)

Goal: the app talks to the rig while the phone **stays on its home WiFi** (and keeps internet).
Android 12 added an optional feature for exactly this, "concurrent local-only and internet
connection".

**What's known**

| fact | tag | source |
|---|---|---|
| Android 12 added Wi-Fi STA/STA concurrency: two Wi-Fi connections at once. One of its functions connects to a local-only network (e.g. an IoT camera) "without disrupting the device's primary internet-providing connection" | VERIFIED | AOSP, *Wi-Fi STA/STA concurrency* |
| It's **optional**: each function is enabled by the phone maker with a resource overlay, **off by default**. Local-only is `config_wifiMultiStaLocalOnlyConcurrencyEnabled`. The Wi-Fi chip must expose two concurrent STA interfaces | VERIFIED | same |
| An app checks support with `WifiManager.isStaConcurrencyForLocalOnlyConnectionsSupported()`, API 31 (Android 12) | VERIFIED | same; Android API reference (via Microsoft's .NET binding docs) |
| On Android 11 and lower, connecting to such a device drops the primary Wi-Fi (and its internet), unless mobile data is there | VERIFIED | AOSP page |
| The connection itself is a `WifiNetworkSpecifier` request (Android 10+). It gives **no internet**. For a request naming one exact network, the user's approval is **stored** per app + access point, so the dialog appears only the first time; SSID/BSSID *patterns* ask every time; "forget network" revokes it | VERIFIED | Android developers, *Wi-Fi Network Request API for peer-to-peer connectivity* |
| A React Native Wi-Fi library uses this to auto-join a device's Wi-Fi while the phone stays on another network, and reports that newer Pixels support local-only concurrency while saying *no* to the related "make-before-break" check | reported, not verified | react-native-wifi-reborn PR #4 |
| Without it, users report the phone dropping the no-internet Wi-Fi, or keeping it "connected, no internet" while apps still can't reach local addresses | reported | Home Assistant Android issue #3961 (open) |

**Not sure**
- **Which phones support it.** No list found. Some Pixels reportedly do; nothing found for Samsung
  or others. It has to be checked per phone (test below).
- **Speed with both networks on one radio.** The rig's AP is 2.4 GHz only (ESP32-S3). If the home
  WiFi is on another 2.4 GHz channel, the phone may have to time-share its radio between the two.
  A 5 GHz home WiFi may run in parallel on chips that can do both bands at once. Untested;
  `ap_channel` ([CONFIG](CONFIG.md)) can move the rig to the home WiFi's channel if it matters.
- **Background:** whether the connection survives the app going to the background.

**What the app has to do** (for when it's built)
1. Check `isStaConcurrencyForLocalOnlyConnectionsSupported()`. If false: warn that the phone's
   own WiFi drops while connected to the rig (mobile data keeps internet).
2. Request the rig's network with the **exact** SSID and password (no pattern, so the approval is
   stored), with the `INTERNET` capability removed.
3. In `onAvailable(network)`, send the rig's traffic **through that `Network`**
   (`network.openConnection(...)` or its socket factory). Not `bindProcessToNetwork`: that would
   pull all of the app's traffic onto the rig, which has no internet. Plain requests to
   `192.168.4.1` would go out on the default network and fail (the likely cause of the
   "can't reach local addresses" reports: ASSUMPTION).
4. `unregisterNetworkCallback` when done, which releases the connection.

The rig needs no change for this: the Core's SoftAP is an ordinary WPA2 network with a fixed SSID.

**Test plan: does your phone support it?** A tiny Android test app (Kotlin, one screen, not the
real app) that:
- shows the Android version and `isStaConcurrencyForLocalOnlyConnectionsSupported()`;
- *Connect* requests the rig's WiFi (or step 8's camera WiFi `x4cam-cam-test`, so it can be
  tested with one camera and no Core);
- fetches `http://192.168.4.1/` over that network and an internet URL over the default network at
  the same time, and shows whether the home WiFi stayed connected.

Not built yet: the Android SDK download host is blocked in the build environment. Build it in
Android Studio, or allow `dl.google.com` for the cloud environment
([OPEN_QUESTIONS Q22](OPEN_QUESTIONS.md)).

Sources:
- AOSP, Wi-Fi STA/STA concurrency: https://source.android.com/docs/core/connect/wifi-sta-sta-concurrency
- Android developers, Wi-Fi Network Request API: https://developer.android.com/develop/connectivity/wifi/wifi-bootstrap
- API level of the check: https://learn.microsoft.com/en-us/dotnet/api/android.net.wifi.wifimanager.isstaconcurrencyforlocalonlyconnectionssupported
- react-native-wifi-reborn PR #4: https://github.com/orca-io/react-native-wifi-reborn/pull/4
- Home Assistant Android issue #3961: https://github.com/home-assistant/android/issues/3961

## 3. SD / file contract

```
/x4/000042-1.jpg … 000042-4.jpg    full-resolution JPEG (5 MP by default), one per camera
```
Flat, no folders, no sidecar (owner, 2026-11-11). Name = persistent shot counter (6 digits) +
camera number; no date, the Core has no working clock. Readable on any PC (FAT32). Each photo
carries the shot's settings (`orientation`, `effect`, …) as a JSON comment inside the JPEG,
format `x4-shot/2`, documented in [Protocol](PROTOCOL.md#sd-layout-fat32); new keys may be added,
existing ones keep their meaning within `x4-shot/2`. The pixels are always the original:
applying portrait crop / dither is the companion's job, until the cameras can do it themselves
([STEPS step 14](STEPS.md#14-effects-on-the-camera-planned-no-firmware-yet)).

## 4. BLE (not v1)

A possible later addition for remote shutter/status from a phone without joining the AP. Not for
photos (too slow).
