// Rewrites links inside docs so they work on the site:
//   POWER.md#risks        -> /rig/power#risks
//   docs/BRINGUP.md       -> /rig/bringup          (links from the repo README)
//   ../core/src/hw/x.h    -> https://github.com/qurle/x4/blob/main/core/src/hw/x.h
// External links and #anchors are left alone. Works on the AST, so code blocks are untouched.
import { DOCS, REPO_BLOB, routeFor } from '../../scripts/doc-map.mjs'

function visit(node, fn) {
  fn(node)
  if (node.children) for (const c of node.children) visit(c, fn)
}

function rewrite(url, fromReadme) {
  if (!url || /^[a-z]+:/i.test(url) || url.startsWith('#') || url.startsWith('//')) return url
  const [pathPart, hash = ''] = url.split('#')
  const anchor = hash ? '#' + hash : ''
  const clean = pathPart.replace(/^\.\//, '')

  // docs/X.md (from README) or X.md (between docs)
  const docName = clean.replace(/^docs\//, '')
  if (DOCS[docName] !== undefined) return routeFor(docName) + anchor
  if (clean === 'docs/' || clean === 'docs') return '/'

  // Anything else is a repo file: resolve relative to docs/ (or to the repo root for README).
  const base = fromReadme ? '' : 'docs/'
  const parts = (base + clean).split('/')
  const stack = []
  for (const p of parts) {
    if (p === '..') stack.pop()
    else if (p && p !== '.') stack.push(p)
  }
  return REPO_BLOB + stack.join('/') + anchor
}

// "POWER.md §2c" / "docs/BRINGUP.md" -> "power.md §2c" / "bringup.md" (how notes. lists pages).
function relabel(link) {
  for (const child of link.children ?? []) {
    if (child.type !== 'text') continue
    child.value = child.value.replace(/^(?:docs\/)?([A-Z_]+\.md)/, (m, name) => {
      const slug = DOCS[name]
      return slug === undefined ? m : slug.split('/').at(-1) + '.md'
    })
  }
}

export function rewriteDocLinks() {
  return (tree, file) => {
    const source = file.data.astro?.frontmatter?.source
    if (!source) return // not a synced doc
    const fromReadme = source === 'docs/README.md'
    visit(tree, (node) => {
      if ((node.type === 'link' || node.type === 'definition') && node.url) {
        node.url = rewrite(node.url, fromReadme)
        if (node.type === 'link') relabel(node)
      }
    })
  }
}
