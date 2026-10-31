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

## 3. SD / file contract

```
/x4/shots/S000042/cam1.jpg … cam4.jpg     full-resolution JPEG (5 MP by default)
/x4/shots/S000042/shot.json               sidecar, format "x4-shot/1"
```
Folder name = persistent shot counter (6 digits). Readable on any PC (FAT32). The sidecar format
is documented in [PROTOCOL.md](PROTOCOL.md#sd-layout-fat32); new keys may be added, existing ones
keep their meaning within `x4-shot/1`.

## 4. BLE (not v1)

A possible later addition for remote shutter/status from a phone without joining the AP. Not for
photos (too slow).
