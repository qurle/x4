# x4cam: notes for Claude sessions

Portable 4-camera rig: M5Stack CoreS3-SE (UI, WiFi AP, SD) + 4× Unit CamS3-5MP + Flashlight Unit +
Mini Button, powered by an Anker power bank through a USB-C breakout rail. Start with
`README.md` and `docs/README.md`; the original brief is `docs/BRIEF.md`.

## How the owner wants to work

- Concise, plain answers. Say "Not sure" when something is a guess, not a fact.
- Never invent hardware facts. Tag them **VERIFIED** (with source), **ASSUMPTION** or
  **Not sure**. Check M5Stack hardware questions with the M5Stack knowledge-base connector
  (`knowledge_search`) before answering. M5's own docs sometimes contradict each other (e.g. the
  SE pin-map page lists Ports A/B/C, the SE schematic has only J3 = Port A); say so when they do.
- No screenshot-based UI development. Build the UI from Figma node data (see below).
- **Figma is the truth** for the UI: where it disagrees with an earlier request or commit, follow
  Figma (owner, 2026-11-01).
- Project name is **x4cam** (`X4_PROJECT_NAME` in `shared/include/x4/version.h`). "Four Eyes" is
  gone for good.
- No battery gauge of any kind (ADR-014). The bank's own indicator is the truth.
- No Cyrillic glyphs in fonts.
- Small steps: the bring-up ladder in `steps/` + `docs/STEPS.md` exists because the full firmware
  is a lot at once.

## Git

- Commit author is the owner: pass `--author="nick <wrl.snk@gmail.com>"` on every commit. The
  committer must stay `Claude <noreply@anthropic.com>` (a stop hook checks that commits are
  verified).
- Don't force-push or rewrite pushed history. After a PR is merged, fast-forward the branch to
  `origin/main` before new work.
- When work is ready, **open a PR and send the owner the link**; follow-up fixes go on as new
  commits to the same PR. The owner merges (owner, 2026-11-01). After a merge, start the next
  work from `origin/main`.

## Build and checks (run what you touched before pushing)

```bash
cd core && pio run && pio test -e native        # core, core-hello, hello-ui + 32 host tests
cd camera && pio run                             # cam1..cam4, cam-hello
cd steps && pio run                              # 15 bring-up envs (slow, ~5 min)
python tools/gen_docs.py --check && python tools/gen_tokens.py --check && python tools/gen_figma_assets.py --check && python tools/gen_assets.py --check
python tools/ui_sim/run.py                       # UI on the PC vs Figma: look at tools/ui_sim/out/index.html
cd site && npm ci && npm run build               # manual site from docs/
```

- Config options live in `shared/include/x4/config_schema.def`; `docs/CONFIG.md` is generated
  (`python tools/gen_docs.py`). Same for error codes (`errors.def`).
- UI tokens: edit `ui/tokens.json`, then `python tools/gen_tokens.py` → `core/src/ui/tokens.h`.
- UI images: `ui/figma/` (pulled) → `python tools/gen_figma_assets.py` (Pillow 13.0.0, pinned: other
  versions resample differently and fail the CI check) → `ui/assets/figma/*.png` →
  `python tools/gen_assets.py` → `core/src/ui/assets/`.
- Toolchain is pinned (pioarduino 55.03.312-1, M5Unified 0.2.29, M5GFX 0.2.36, LVGL 9.6.0); see
  `docs/FIRMWARE.md#pinned-versions`. If PlatformIO downloads fail in the sandbox, check the
  proxy notes in `/root/.ccr/README.md` first.

## Safety rules that code must keep

- The Core is fed backwards through Port A: its 5 V outputs (BUS_OUT_EN, USB_OTG_EN) stay off,
  checked at boot and every 2 s (`core/src/hw/power_safety.cpp`, `steps/include/core_common.h`).
- Never USB and rail on the same board at the same time.
- Flash pin G2 is LOW from static init; pulse cap 150 ms, cooldown ≥ 8 s (`x4/safety.h`).

## Figma

File `fgEvP6Ms0bOAbWxfkFki4R` ("x4"). Sections: `73:368` "states" (viewfinder, tap menu,
settings, saving, error), `83:2147` "settings states" (tile components: orientation
portrait/horizontal, cam delay 0–1 s slider, style normal/dithered, flashlight flash/no flash).
Saving and Error overlays are components too (IDs: find them in `ui/figma/components.json`).

Use the REST API, not the Figma MCP (the MCP's Starter plan limit runs out after a couple of
calls):

```bash
python tools/figma_pull.py              # needs FIGMA_TOKEN (read-only) in the environment
```

It writes `ui/figma/components.json`, `ui/figma/nodes/<id>.json` (raw geometry, fills, effects,
text), `ui/figma/fills/*.png` (image fills used by `gen_figma_assets.py`) and
`ui/figma/export/*.png` (reference renders only). Commit the output so design changes show up as
diffs. `api.figma.com` is allowed in this environment's network policy. The file's components are
unpublished, so `components.json` is built from the node responses. The `/images` (renders)
endpoint can answer 429 with a Retry-After of days (seen 2026-11-01: ~111 h); the script then
skips renders and keeps the existing PNGs (`--no-renders` skips them on purpose). 135 of 154
renders are in the repo. `/nodes` can be rate-limited the same way (seen 2026-11-01, ~109 h);
`--fills-only` still downloads image fills using the committed node JSON.

## In flight (update or delete this section as it changes)

1. Video is not implemented; `Status.videoMode` and `Status.recording` are always false. The
   footer's record block (dot + `-:--` / timer) shows only in video mode, as in Figma's Footer
   State "Video" (owner, 2026-11-01: Figma is the truth), so it is hidden in v1.
2. UI positions are checked against Figma in the simulator (`tools/ui_sim`, docs/DEVELOP.md §3).
   Still to do on the panel: speed, colours, touch feel.
3. Optional: split the CI `steps / all` job into parallel jobs (it takes ~10 of the 10.5 min).
