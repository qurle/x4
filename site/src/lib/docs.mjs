// Every page of the site that comes from docs/*.md: group, order, label, one-line blurb, route.
// Used by the pages, the navigation and the link rewriter. A doc missing here fails the build
// (see checkMapped), so new docs can't silently drop off the site.

export const REPO = 'https://github.com/qurle/x4'
export const REPO_BLOB = REPO + '/blob/main/'

export const GROUPS = [
  {
    name: 'Build the rig',
    docs: [
      ['STEPS.md', 'Play-through', "From a box of parts to the full rig"],
      ['BRINGUP.md', 'Bring-up', "Flashing rules and full-firmware tests"],
      ['WIRING.md', 'Wiring', "Diagram, pins, connectors"],
      ['POWER.md', 'Power', "Why it won't burn, and the risks"],
      ['HARDWARE.md', 'Hardware', "Parts, pins, what's verified"],
      ['ENCLOSURE.md', 'Enclosure', "Foam board now, acrylic later"],
      ['MEASUREMENTS.md', 'Measurements', "Tables to fill on the bench"],
    ],
  },
  {
    name: 'Code',
    docs: [
      ['DEVELOP.md', 'Develop', "Build, test and simulate on a computer"],
      ['FIRMWARE.md', 'Firmware', "Tasks, states, memory, inputs"],
      ['UI.md', 'UI', "From Figma to the screen"],
      ['PROTOCOL.md', 'Protocol', "Trigger, clock sync, HTTP, SD layout"],
      ['CONFIG.md', 'Config', "Every option and its limits"],
      ['LOGGING.md', 'Logging', "Log format and error codes"],
      ['COMPANION.md', 'Companion', "Web page now, app later"],
    ],
  },
  {
    name: 'When something breaks',
    docs: [
      ['TROUBLESHOOTING.md', 'Troubleshooting', "Symptom, cause, check, fix"],
      ['DEBUGGING.md', 'Debugging', "Serial, diagnostics, measuring skew"],
    ],
  },
  {
    name: 'Background',
    docs: [
      ['DECISIONS.md', 'Decisions', "What was chosen, and why"],
      ['OPEN_QUESTIONS.md', 'Open questions', "Still open, with the defaults used"],
      ['RESEARCH.md', 'Research', "Findings with sources"],
      ['BRIEF.md', 'Brief', "The original brief"],
    ],
  },
]

// docs/README.md is the docs index; on the site the landing page plays that role.
const LANDING = 'README.md'

const slugOf = (file) => file.replace(/\.md$/, '').toLowerCase().replace(/_/g, '-')

export const ALL = GROUPS.flatMap((g) =>
  g.docs.map(([file, label, blurb]) => ({ file, label, blurb, group: g.name, slug: slugOf(file) })),
)

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
