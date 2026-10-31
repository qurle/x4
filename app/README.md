# app/ — companion app (later; stub)

Not on the critical path. Plan (docs/COMPANION.md):

1. **Web debug page first** — already served by the Core at `http://192.168.4.1/` (same origin as
   the API: no CORS or mixed-content issues).
2. **Flutter later** (Android + iOS both required): joins the Core's AP programmatically
   (`WifiNetworkSpecifier` on Android 10+, `NEHotspotConfiguration` on iOS), lists and downloads
   photos through the API in docs/PROTOCOL.md, does post-processing (alignment, dithering, GIFs).

Do not start the Flutter project until the owner asks. The contract it must follow:
docs/PROTOCOL.md (HTTP API) and docs/COMPANION.md (SD layout, sidecar format, platform notes).
