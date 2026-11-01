// Copies ../docs/*.md into src/pages/ (paths from doc-map.mjs) and adds a frontmatter title
// taken from the first "# heading". Runs before `npm run dev` / `npm run build`.
// docs/ stays the source of truth (readable on GitHub); src/pages/ content is generated and
// gitignored. Links between docs are rewritten at render time by
// src/remark-plugins/rewrite-doc-links.mjs.
import fs from 'node:fs'
import path from 'node:path'
import { fileURLToPath } from 'node:url'
import { DOCS } from './doc-map.mjs'

const here = path.dirname(fileURLToPath(import.meta.url))
const site = path.resolve(here, '..')
const repo = path.resolve(site, '..')
const docsDir = path.join(repo, 'docs')
const pagesDir = path.join(site, 'src', 'pages')

// Clean previously generated pages (keep the folder-listing route).
for (const entry of fs.readdirSync(pagesDir)) {
  if (entry === '[...folder].astro') continue
  fs.rmSync(path.join(pagesDir, entry), { recursive: true, force: true })
}

function titleOf(md, fallback) {
  const m = md.match(/^#\s+(.+)$/m)
  return (m ? m[1] : fallback).replace(/[`*_]/g, '').trim()
}

function withFrontmatter(md, title, source) {
  const fm = ['---', `title: ${JSON.stringify(title)}`, `source: ${JSON.stringify(source)}`, '---', ''].join('\n')
  return fm + md
}

let count = 0
for (const [file, slug] of Object.entries(DOCS)) {
  const src = path.join(docsDir, file)
  if (!fs.existsSync(src)) {
    console.warn(`sync-docs: missing docs/${file}`)
    continue
  }
  let md = fs.readFileSync(src, 'utf8')
  let title = titleOf(md, file.replace(/\.md$/, ''))

  if (file === 'README.md') {
    // Root page: the repo README (overview, status, quick start) followed by the docs index.
    const readme = fs.readFileSync(path.join(repo, 'README.md'), 'utf8')
      .replace(/^# .*\n/, '')
      .replace(/!\[\]\(\/\.github\/assets\/cover\.png\)\n?/, '')
    md = `# x4cam\n${readme}\n${md.replace(/^# .*\n/, '## Manual\n')}`
    title = 'start here'
  }

  const out = path.join(pagesDir, slug + '.md')
  fs.mkdirSync(path.dirname(out), { recursive: true })
  fs.writeFileSync(out, withFrontmatter(md, title, `docs/${file}`))
  count++
}
console.log(`sync-docs: ${count} pages from docs/ -> src/pages/`)
