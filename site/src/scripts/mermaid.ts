// Renders ```mermaid code blocks (wiring diagram, state machine) as SVG.
// mermaid is a large library, so it is only downloaded on pages that contain a diagram.
// Re-renders when the theme switcher changes data-theme on <html>.

const themeFor = (t?: string) => (t === 'light' ? 'neutral' : 'dark')

export async function initMermaid() {
	const blocks = Array.from(document.querySelectorAll<HTMLElement>('pre > code.language-mermaid'))
	if (!blocks.length) return
	const { default: mermaid } = await import('mermaid')

	// Replace each <pre> with a container that keeps the source for re-rendering.
	const hosts = blocks.map((code) => {
		const host = document.createElement('div')
		host.className = 'mermaid-diagram'
		host.dataset.source = code.textContent ?? ''
		code.parentElement!.replaceWith(host)
		return host
	})

	let run = 0
	async function render() {
		const id = ++run
		mermaid.initialize({
			startOnLoad: false,
			theme: themeFor(document.documentElement.dataset.theme),
			fontFamily: 'inherit',
			securityLevel: 'strict',
		})
		for (const [i, host] of hosts.entries()) {
			try {
				const { svg } = await mermaid.render(`mmd-${id}-${i}`, host.dataset.source ?? '')
				if (id === run) host.innerHTML = svg
			} catch {
				host.textContent = host.dataset.source ?? ''
				host.classList.add('mermaid-error')
			}
		}
	}

	await render()
	new MutationObserver(render).observe(document.documentElement, {
		attributes: true,
		attributeFilter: ['data-theme'],
	})
}
