// @ts-check
// x4cam website: a landing page plus every docs/*.md, read straight from ../docs (the docs stay
// the source of truth and read fine on GitHub). Page list and order: src/lib/docs.mjs.
import { defineConfig } from 'astro/config'
import { rewriteDocLinks } from './src/lib/rewrite-links.mjs'

export default defineConfig({
  markdown: {
    syntaxHighlight: false,
    remarkPlugins: [rewriteDocLinks],
  },
})
