// Remark plugin: makes links inside docs/*.md work on the site.
//   STEPS.md#level-0     -> /steps/#level-0
//   README.md            -> /
//   ../tools/ui_sim/     -> https://github.com/qurle/x4/blob/main/tools/ui_sim/
// A link whose text is just a file name ("WIRING.md") gets the page's label ("Wiring").
// External links and #anchors stay as they are. Works on the syntax tree, so code is untouched.
import path from 'node:path'
import { REPO_BLOB, labelFor, routeFor } from './docs.mjs'

function visit(node, fn) {
  fn(node)
  for (const c of node.children ?? []) visit(c, fn)
}

function rewrite(url) {
  if (!url || /^[a-z]+:/i.test(url) || url.startsWith('#') || url.startsWith('//') || url.startsWith('/')) return url
  const [p, hash] = url.split('#')
  const anchor = hash ? '#' + hash : ''
  const route = routeFor(p.replace(/^\.\//, ''))
  if (route) return route + anchor
  // Anything else is a file in the repo, relative to docs/.
  const target = path.posix.normalize('docs/' + p).replace(/^\.\//, '')
  return REPO_BLOB + target + anchor
}

export function rewriteDocLinks() {
  return (tree, file) => {
    const src = String(file.path ?? file.history?.[0] ?? '')
    if (!/[\\/]docs[\\/][^\\/]+\.md$/.test(src)) return // only docs/*.md
    visit(tree, (node) => {
      if ((node.type !== 'link' && node.type !== 'definition') || !node.url) return
      node.url = rewrite(node.url)
      if (node.type !== 'link') return
      const only = node.children?.length === 1 ? node.children[0] : null
      if (only && only.type === 'text') {
        only.value = only.value.replace(/^(?:docs\/)?([A-Z_]+\.md)\b/, (m, f) => labelFor(f) ?? m)
      }
    })
  }
}
