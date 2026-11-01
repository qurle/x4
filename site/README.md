# x4cam website (site/)

A landing page plus every `docs/*.md`, rendered with [Astro](https://astro.build). Dark, like the
device's screen (`#1E1E1E`). Instrument Serif for headings, Inter for reading, Space Mono for code,
tables and short labels. Lime (`#DDFF00`) is the only accent: primary buttons, link underlines,
selection and focus. Motion is short and ease-out, and only where it explains something (see the top
of `src/styles/site.css`).

**`docs/` stays the source of truth.** The site reads the Markdown where it is
(`src/content.config.ts`) and nothing is copied. Edit the docs; the site follows.

| file | what it does |
|---|---|
| `src/lib/docs.mjs` | the page list: groups, order, labels, one-line blurbs, routes (`STEPS.md` → `/steps/`). A doc missing here fails the build |
| `src/lib/rewrite-links.mjs` | links between docs → site routes; links to repo files → GitHub; a link reading `WIRING.md` → "Wiring" |
| `src/pages/index.astro` | the landing page; each doc's one-line blurb is in `src/lib/docs.mjs` |
| `src/pages/[slug].astro`, `src/layouts/` | doc pages: article, page list, next / previous |
| `src/styles/site.css` | all styling |
| `src/scripts/copy.js` | a Copy button on code blocks |
| `src/scripts/mermaid.js` | `mermaid` blocks as diagrams, loaded only on pages that have one; wide ones fit the column and open full size on click |
| `public/screens/`, `public/og.png` | the device screens on the landing page, rendered by the UI simulator. Refresh: `python tools/ui_sim/run.py preview_photo settings saving_3of4 preview_portrait`, then `python site/scripts/images.py` |
| `public/fonts/` | the three fonts as WOFF2 (Inter trimmed to the characters the docs use), with their SIL Open Font License files |

Adding a doc: create `docs/NEW.md`, add it with a short blurb to `src/lib/docs.mjs`, and give it a
row in `docs/README.md`.

## Commands

Node ≥ 22.12.

```bash
cd site
```
```bash
npm ci
```
```bash
npm run dev
```
`npm run build` writes `site/dist/`; CI builds it on every push.

## Deploy

Static output. On Vercel: import the repo with **Root Directory = `site`** and framework Astro.
The site reads `../docs`, which is in the same checkout.
