// docs/*.md, read where they are (one level up). The id is the file name, e.g. "STEPS.md".
import { defineCollection } from 'astro:content'
import { glob } from 'astro/loaders'

export const collections = {
  docs: defineCollection({
    loader: glob({ pattern: '*.md', base: '../docs', generateId: ({ entry }) => entry }),
  }),
}
