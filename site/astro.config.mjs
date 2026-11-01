// @ts-check
import { defineConfig } from 'astro/config'
import path from 'path'
import icon from 'astro-icon'
import mdx from '@astrojs/mdx'
import { unified } from '@astrojs/markdown-remark'
import json5Plugin from "vite-plugin-json5"

import { autoNoteLayout } from './src/remark-plugins/auto-doc-layout.mjs'
import { rewriteDocLinks } from './src/remark-plugins/rewrite-doc-links.mjs'


// x4 manual: the notes. template (github.com/qurle/notes) fed from ../docs/*.md.
// https://astro.build/config
export default defineConfig({
    integrations: [icon({
        include: {
            materialSymbols: [
                'arrow-back',
                'square-outline',
                'circle-outline',
                'close',
            ]
        }
    }), mdx()],
    markdown: {
        syntaxHighlight: false,
        processor: unified({ remarkPlugins: [autoNoteLayout, rewriteDocLinks] }),
    },
    // Cause tsconfig is not fucking enough???
    vite: {
        plugins: [json5Plugin()],
        resolve: {
            alias: {
                '@styles': path.resolve('./src/styles'),
            },
        },
    },
});