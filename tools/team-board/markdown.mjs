const DANGEROUS_SCHEME = /^(?:javascript|data|vbscript|file|blob):/i;

export function safeMarkdownUrl(value, internalRoot = '') {
  const raw = String(value ?? '').trim();
  if (!raw || DANGEROUS_SCHEME.test(raw) || /[\u0000-\u001f\u007f]/.test(raw)) return null;
  if (/^(?:https?:|mailto:)/i.test(raw)) return raw;
  if (raw.startsWith('/') && !raw.startsWith('//')) {
    if (!internalRoot) return raw;
    return `${internalRoot}${raw}`;
  }
  if (raw.startsWith('./') || raw.startsWith('../') || raw.startsWith('#')) return internalRoot ? `${internalRoot}${raw}` : raw;
  return null;
}

export function parseInlineMarkdown(source) {
  const tokens = [];
  const pattern = /(`+)([\s\S]*?)\1|!?(\[[^\]]*\])\(([^)\s]+)(?:\s+["']([^"']*)["'])?\)|\*\*([^*]+)\*\*|__([^_]+)__|\*([^*]+)\*|_([^_]+)_/g;
  let cursor = 0;
  const addText = (value) => { if (value) tokens.push({ type: 'text', value }); };
  for (let match = pattern.exec(source); match; match = pattern.exec(source)) {
    addText(source.slice(cursor, match.index));
    if (match[1]) tokens.push({ type: 'code', value: match[2] });
    else if (match[3]) {
      const label = match[3].slice(1, -1);
      const url = safeMarkdownUrl(match[4]);
      tokens.push(url ? { type: 'link', label, url, title: match[5] || '' } : { type: 'text', value: match[0] });
    } else if (match[6] || match[7]) tokens.push({ type: 'strong', value: match[6] || match[7] });
    else tokens.push({ type: 'em', value: match[8] || match[9] });
    cursor = match.index + match[0].length;
  }
  addText(source.slice(cursor));
  return tokens;
}

export function parseMarkdown(source) {
  const lines = String(source ?? '').replace(/\r\n?/g, '\n').split('\n');
  const blocks = [];
  let i = 0;
  while (i < lines.length) {
    const line = lines[i];
    if (!line.trim()) { i += 1; continue; }
    const fence = line.match(/^\s*(```+|~~~+)\s*([^`]*)$/);
    if (fence) {
      const marker = fence[1]; const body = [];
      i += 1;
      while (i < lines.length && !new RegExp(`^\\s*${marker[0]}{${marker.length},}\\s*$`).test(lines[i])) body.push(lines[i++]);
      if (i < lines.length) i += 1;
      blocks.push({ type: 'codeblock', value: body.join('\n'), language: fence[2].trim() }); continue;
    }
    const heading = line.match(/^\s{0,3}(#{1,6})\s+(.+?)\s*#*\s*$/);
    if (heading) { blocks.push({ type: 'heading', level: heading[1].length, children: parseInlineMarkdown(heading[2]) }); i += 1; continue; }
    if (/^\s*>/.test(line)) {
      const quote = []; while (i < lines.length && /^\s*>/.test(lines[i])) quote.push(lines[i++].replace(/^\s*>\s?/, ''));
      blocks.push({ type: 'blockquote', children: parseMarkdown(quote.join('\n')).blocks }); continue;
    }
    if (/^\s*(?:[-+*]|\d+[.)])\s+/.test(line)) {
      const ordered = /^\s*\d/.test(line); const items = [];
      while (i < lines.length && /^\s*(?:[-+*]|\d+[.)])\s+/.test(lines[i])) items.push(parseInlineMarkdown(lines[i++].replace(/^\s*(?:[-+*]|\d+[.)])\s+/, '')));
      blocks.push({ type: 'list', ordered, items }); continue;
    }
    const paragraph = [line]; i += 1;
    while (i < lines.length && lines[i].trim() && !/^\s*(?:#{1,6}\s|>|[-+*]\s|\d+[.)]\s|```|~~~)/.test(lines[i])) paragraph.push(lines[i++]);
    blocks.push({ type: 'paragraph', children: parseInlineMarkdown(paragraph.join('\n')) });
  }
  return { type: 'document', blocks };
}
