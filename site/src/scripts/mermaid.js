// Renders ```mermaid blocks (wiring diagram, state machine) as SVG. Mermaid is large, so it is
// only downloaded on pages that have a diagram. Drawn in the site's dark colours.
// A diagram wider than the column is shown fitted; a click shows it at full size (scrolls sideways).
function zoomable(host) {
  const svg = host.querySelector('svg')
  const full = svg?.viewBox?.baseVal?.width ?? 0
  host.classList.remove('zoomable', 'zoomed')
  host.nextElementSibling?.classList.contains('diagram-hint') && host.nextElementSibling.remove()
  if (!svg || full <= host.clientWidth * 1.15) return
  host.classList.add('zoomable')
  host.onclick = () => {
    const on = host.classList.toggle('zoomed')
    svg.style.maxWidth = on ? 'none' : `${full}px`
    svg.style.width = on ? `${full}px` : '100%'
  }
  const hint = document.createElement('p')
  hint.className = 'diagram-hint'
  hint.textContent = 'Click the diagram to see it full size.'
  host.after(hint)
}

export async function renderDiagrams() {
  const blocks = [...document.querySelectorAll('pre > code.language-mermaid')]
  if (!blocks.length) return
  const { default: mermaid } = await import('mermaid')
  const hosts = blocks.map((code) => {
    const host = document.createElement('div')
    host.className = 'diagram'
    host.dataset.source = code.textContent ?? ''
    code.parentElement.replaceWith(host)
    return host
  })
  let run = 0
  async function render() {
    const id = ++run
    mermaid.initialize({
      startOnLoad: false,
      securityLevel: 'strict',
      theme: 'base',
      fontFamily: 'Space Mono, monospace',
      themeVariables: { background: '#1E1E1E', primaryColor: '#1E1E1E', primaryTextColor: '#D1D1D1', primaryBorderColor: '#D1D1D1', lineColor: '#D1D1D1', clusterBkg: 'transparent', clusterBorder: '#D1D1D1', fontSize: '13px' },
    })
    for (const [i, host] of hosts.entries()) {
      try {
        const { svg } = await mermaid.render(`diagram-${id}-${i}`, host.dataset.source)
        if (id !== run) continue
        host.innerHTML = svg
        zoomable(host)
      } catch {
        host.textContent = host.dataset.source
      }
    }
  }
  await render()
}
