// Where each docs/*.md file lives on the manual site. One table, used by the sync script
// (copies files) and by the remark plugin (rewrites links between docs).
// Folder listings are alphabetical (notes. template), so the reading order lives on the root
// page "start here" (built from the repo README + docs/README.md).

export const REPO_BLOB = 'https://github.com/qurle/x4/blob/main/'

export const DOCS = {
  'README.md': 'start here', // root page
  'BRINGUP.md': 'rig/bringup',
  'WIRING.md': 'rig/wiring',
  'POWER.md': 'rig/power',
  'HARDWARE.md': 'rig/hardware',
  'MEASUREMENTS.md': 'rig/measurements',
  'ENCLOSURE.md': 'rig/enclosure',
  'FIRMWARE.md': 'firmware/overview',
  'PROTOCOL.md': 'firmware/protocol',
  'CONFIG.md': 'firmware/config',
  'LOGGING.md': 'firmware/logging',
  'UI.md': 'firmware/ui',
  'COMPANION.md': 'firmware/companion',
  'TROUBLESHOOTING.md': 'help/troubleshooting',
  'DEBUGGING.md': 'help/debugging',
  'RESEARCH.md': 'project/research',
  'DECISIONS.md': 'project/decisions',
  'OPEN_QUESTIONS.md': 'project/open-questions',
  'BRIEF.md': 'project/brief',
}

// URL path for a doc file name (as linked from inside docs/), e.g. "POWER.md" -> "/rig/power".
export function routeFor(docName) {
  const slug = DOCS[docName]
  if (slug === undefined) return undefined
  return slug === 'start here' ? '/start%20here' : '/' + slug
}
