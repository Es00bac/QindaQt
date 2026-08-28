const dangerousScheme = /^(?:javascript|data|vbscript|file|blob):/i;
const internalMessagePath = /^ops\/team\/messages\/_?[a-z0-9][a-z0-9-]*\/\d+-[^/]+\.md(?:\.done)?$/;

export function messageLabel(entry) {
  const label = String(entry?.label || '');
  return entry?.closed && !/\(closed\)$/.test(label) ? `${label} (closed)` : label;
}

export function safeUrl(value) {
  const url = String(value || '').trim();
  if (!url || dangerousScheme.test(url) || /[\u0000-\u001f\u007f]/.test(url)) return null;
  if (/^(?:https?:|mailto:)/i.test(url)) return url;
  if (internalMessagePath.test(url) || url.startsWith('/') || url.startsWith('./') || url.startsWith('../') || url.startsWith('#')) return url;
  return null;
}

function linkHref(url, root) {
  const match = url.match(/^ops\/team\/messages\/([^/]+)\/(\d+-[^/]+\.md(?:\.done)?)$/);
  if (match) return `/api/messages/${encodeURIComponent(match[1])}/${encodeURIComponent(match[2])}`;
  return url.startsWith('/') || url.startsWith('.') || url.startsWith('#') ? root + url : url;
}

export function renderMarkdown(source, root = '') {
  const output = document.createDocumentFragment();
  const lines = String(source || '').replace(/\r\n?/g, '\n').split('\n');
  let lineIndex = 0;
  const inline = (value) => {
    const fragment = document.createDocumentFragment();
    const pattern = /(`+)([\s\S]*?)\1|!?\[([^\]]+)\]\(([^)\s]+)(?:\s+["']([^"']*)["'])?\)|\*\*([^*]+)\*\*|__([^_]+)__|\*([^*]+)\*|_([^_]+)_/g;
    let cursor = 0;
    for (let match; (match = pattern.exec(value));) {
      if (match.index > cursor) fragment.append(document.createTextNode(value.slice(cursor, match.index)));
      if (match[1]) {
        const code = document.createElement('code'); code.textContent = match[2]; fragment.append(code);
      } else if (match[3]) {
        const url = safeUrl(match[4]);
        if (!url) fragment.append(document.createTextNode(match[0]));
        else {
          const anchor = document.createElement('a'); anchor.textContent = match[3]; anchor.href = linkHref(url, root);
          if (/^https?:/i.test(url)) { anchor.target = '_blank'; anchor.rel = 'noopener noreferrer nofollow'; }
          fragment.append(anchor);
        }
      } else {
        const element = document.createElement(match[6] || match[7] ? 'strong' : 'em');
        element.textContent = match[6] || match[7] || match[8] || match[9]; fragment.append(element);
      }
      cursor = match.index + match[0].length;
    }
    if (cursor < value.length) fragment.append(document.createTextNode(value.slice(cursor)));
    return fragment;
  };
  while (lineIndex < lines.length) {
    const line = lines[lineIndex];
    if (!line.trim()) { lineIndex += 1; continue; }
    const fence = line.match(/^\s*(```+|~~~+)\s*(.*)$/);
    if (fence) {
      const element = document.createElement('pre'); const code = document.createElement('code'); const body = [];
      lineIndex += 1;
      while (lineIndex < lines.length && !new RegExp(`^\\s*${fence[1][0]}{${fence[1].length},}\\s*$`).test(lines[lineIndex])) body.push(lines[lineIndex++]);
      if (lineIndex < lines.length) lineIndex += 1;
      code.textContent = body.join('\n'); element.append(code); output.append(element); continue;
    }
    const heading = line.match(/^\s{0,3}(#{1,6})\s+(.+?)\s*#*\s*$/);
    if (heading) { const element = document.createElement(`h${heading[1].length}`); element.append(inline(heading[2])); output.append(element); lineIndex += 1; continue; }
    if (/^\s*>/.test(line)) { const element = document.createElement('blockquote'); element.append(inline(line.replace(/^\s*>\s?/, ''))); output.append(element); lineIndex += 1; continue; }
    const list = line.match(/^\s*([-+*]|\d+[.)])\s+(.+)/);
    if (list) {
      const element = document.createElement(/^\d/.test(list[1]) ? 'ol' : 'ul');
      while (lineIndex < lines.length) { const item = lines[lineIndex].match(/^\s*([-+*]|\d+[.)])\s+(.+)/); if (!item) break; const li = document.createElement('li'); li.append(inline(item[2])); element.append(li); lineIndex += 1; }
      output.append(element); continue;
    }
    const paragraph = document.createElement('p'); paragraph.append(inline(line)); lineIndex += 1;
    while (lineIndex < lines.length && lines[lineIndex].trim() && !/^\s*(?:#{1,6}\s|>|[-+*]\s|\d+[.)]\s|```|~~~)/.test(lines[lineIndex])) { paragraph.append(document.createElement('br'), inline(lines[lineIndex++])); }
    output.append(paragraph);
  }
  return output;
}
