# x4 manual (site/)

The docs in `../docs/*.md` rendered as a small static site, built on
[notes.](https://github.com/qurle/notes) (Astro, MIT). Keyboard browsing, fuzzy search,
serif / mono / sans fonts and light / dark / digital themes come from the template.

**`docs/` stays the source of truth.** Edit the Markdown there (it also reads fine on GitHub);
the site is generated from it:

- `scripts/sync-docs.mjs` copies each doc into `src/pages/<folder>/<page>.md` (gitignored) and adds
  a frontmatter title. The folder for each file is in `scripts/doc-map.mjs`:
  `rig/` (bring-up, wiring, power, hardware, measurements, enclosure), `firmware/` (overview,
  protocol, config, logging, ui, companion), `help/` (troubleshooting, debugging), `project/`
  (research, decisions, open questions, brief). The root page *start here* is the repo README
  plus the docs index.
- `src/remark-plugins/rewrite-doc-links.mjs` rewrites links at render time: `POWER.md#x` →
  `/rig/power#x` (label `power.md`), and repo paths like `../core/src/hw/flash.h` → GitHub.
- ```` ```mermaid ```` blocks render as diagrams (`src/scripts/mermaid.ts`, loaded only on pages
  that have one, re-rendered when the theme changes).
- Wide tables scroll sideways (`src/styles/manual.sass`).

Adding a doc: create `docs/NEW.md`, add it to `scripts/doc-map.mjs`, link it from `docs/README.md`.

## Commands

Node ≥ 22.12.

```bash
cd site && npm i
```
```bash
npm run dev
```
```bash
npm run build
```

`dev` and `build` run the sync first. Output goes to `site/dist/`. CI builds it on every push.

## Deploy

Static output. On Vercel: import the repo, set **Root Directory = `site`**, framework Astro
(the sync script reads `../docs`, which is part of the same checkout). Site name, description,
default theme and footer are in `notes.settings.jsonc`; `indexable` is `false` by default.
The OG images and favicons in `public/` are still the notes. ones: replace them if you publish.
