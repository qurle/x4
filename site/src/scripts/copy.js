// A "Copy" button on every code block (not on diagrams). It confirms with "Copied" for a moment.
export function addCopyButtons() {
  if (!navigator.clipboard) return
  for (const pre of document.querySelectorAll('.prose pre')) {
    const code = pre.querySelector('code')
    if (!code || code.classList.contains('language-mermaid')) continue
    const wrap = document.createElement('div')
    wrap.className = 'code'
    pre.replaceWith(wrap)
    wrap.append(pre)
    const button = document.createElement('button')
    button.type = 'button'
    button.className = 'copy'
    button.textContent = 'Copy'
    let timer
    button.addEventListener('click', async () => {
      try {
        await navigator.clipboard.writeText(code.textContent.replace(/\n$/, ''))
        button.textContent = 'Copied'
        button.classList.add('done')
        clearTimeout(timer)
        timer = setTimeout(() => {
          button.textContent = 'Copy'
          button.classList.remove('done')
        }, 1400)
      } catch {
        button.textContent = 'Select and copy'
      }
    })
    wrap.append(button)
  }
}
