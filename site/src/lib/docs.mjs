// Every page of the site that comes from docs/*.md: group, order, short label, route.
// Used by the pages, the navigation and the link rewriter. A doc missing here fails the build
// (see checkMapped), so new docs can't silently drop off the site.

export const REPO = 'https://github.com/qurle/x4'
export const REPO_BLOB = REPO + '/blob/main/'

export const GROUPS = [
  {
    name: 'Build the rig',
    docs: [
      ['STEPS.md', 'Play-through'],
      ['BRINGUP.md', 'Bring-up'],
      ['WIRING.md', 'Wiring'],
      ['POWER.md', 'Power'],
      ['HARDWARE.md', 'Hardware'],
      ['ENCLOSURE.md', 'Enclosure'],
      ['MEASUREMENTS.md', 'Measurements'],
    ],
  },
  {
    name: 'Code',
    docs: [
      ['DEVELOP.md', 'Develop'],
      ['FIRMWARE.md', 'Firmware'],
      ['UI.md', 'UI'],
      ['PROTOCOL.md', 'Protocol'],
      ['CONFIG.md', 'Config'],
      ['LOGGING.md', 'Logging'],
      ['COMPANION.md', 'Companion'],
    ],
  },
  {
    name: 'When something breaks',
    docs: [
      ['TROUBLESHOOTING.md', 'Troubleshooting'],
      ['DEBUGGING.md', 'Debugging'],
    ],
  },
  {
    name: 'Background',
    docs: [
      ['DECISIONS.md', 'Decisions'],
      ['OPEN_QUESTIONS.md', 'Open questions'],
      ['RESEARCH.md', 'Research'],
      ['BRIEF.md', 'Brief'],
    ],
  },
]

// docs/README.md is the docs index; on the site the landing page plays that role.
const LANDING = 'README.md'

const slugOf = (file) => file.replace(/\.md$/, '').toLowerCase().replace(/_/g, '-')

export const ALL = GROUPS.flatMap((g) => g.docs.map(([file, label]) => ({ file, label, group: g.name, slug: slugOf(file) })))

export function routeFor(file) {
  if (file === LANDING) return '/'
  const d = ALL.find((x) => x.file === file)
  return d ? '/' + d.slug + '/' : undefined
}

export function labelFor(file) {
  return ALL.find((x) => x.file === file)?.label
}

export function checkMapped(files) {
  const missing = files.filter((f) => f !== LANDING && !ALL.some((d) => d.file === f))
  if (missing.length)
    throw new Error(`docs not on the site: ${missing.join(', ')}. Add them to site/src/lib/docs.mjs.`)
}
