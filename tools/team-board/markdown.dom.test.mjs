import assert from 'node:assert/strict';
import test from 'node:test';

// The board UI must stay dependency-free, so this suite drives the browser
// renderer against a minimal DOM stub implementing exactly the surface
// renderMarkdown uses: createDocumentFragment, createElement, createTextNode,
// append, and the textContent/href/target/rel properties.
function textNode(value) {
  return { kind: 'text', value: String(value) };
}

function element(tagName) {
  return {
    kind: 'element',
    tagName: tagName.toUpperCase(),
    children: [],
    href: '',
    target: '',
    rel: '',
    textValue: '',
    append(...nodes) {
      this.children.push(...nodes.map((node) => (typeof node === 'string' ? textNode(node) : node)));
    },
    get textContent() {
      return this.textValue;
    },
    set textContent(value) {
      this.textValue = String(value);
    },
  };
}

function fragment() {
  return {
    kind: 'fragment',
    children: [],
    append(...nodes) {
      this.children.push(...nodes.map((node) => (typeof node === 'string' ? textNode(node) : node)));
    },
  };
}

globalThis.document = {
  createDocumentFragment: fragment,
  createElement: element,
  createTextNode: textNode,
};

const collect = (node, visit) => {
  const found = [];
  const walk = (current) => {
    if (visit(current) && current !== node) found.push(current);
    for (const child of current.children ?? []) walk(child);
  };
  walk(node);
  return found;
};

const textOf = (node) => collect(node, (current) => current.kind === 'text').map((leaf) => leaf.value).join('');
const anchors = (node) => collect(node, (current) => current.kind === 'element' && current.tagName === 'A');
const findText = (node, needle) => collect(node, (current) => current.kind === 'element').find((current) => current.tagName !== 'A' && textOf(current).includes(needle));

test('renders safe external and natural internal links with raw HTML kept inert', async () => {
  const { messageLabel, renderMarkdown, safeUrl } = await import('./public/markdown.js');
  const rendered = renderMarkdown('# Thread\n\nA [external](https://example.com) and a [reply](ops/team/messages/_closed-thread/2-closed-reply.md.done) plus a [bad](javascript:alert(1)) one.\n\n<script>alert(1)</script>\n\n- item `code` **bold**\n\n> quoted\n\n```\npassive\n```');
  const external = anchors(rendered).find((anchor) => anchor.href === 'https://example.com');
  assert.ok(external);
  assert.equal(external.target, '_blank');
  assert.equal(external.rel, 'noopener noreferrer nofollow');
  const internal = anchors(rendered).find((anchor) => anchor.textContent === 'reply');
  assert.equal(internal.href, '/api/messages/_closed-thread/2-closed-reply.md.done');
  assert.equal(anchors(rendered).filter((anchor) => anchor.href.includes('javascript')).length, 0, 'dangerous scheme never becomes a link');
  assert.ok(findText(rendered, '<script>alert(1)</script>'), 'raw HTML stays literal text');
  assert.equal(collect(rendered, (node) => node.tagName === 'SCRIPT').length, 0);
  assert.deepEqual(collect(rendered, (node) => node.kind === 'element').map((node) => node.tagName), [
    'H1', 'P', 'A', 'A', 'P', 'UL', 'LI', 'CODE', 'STRONG', 'BLOCKQUOTE', 'PRE', 'CODE',
  ], 'document-order structure matches the documented subset; the rejected link renders as plain text');
  assert.equal(safeUrl('javascript:alert(1)'), null);
  assert.equal(safeUrl('data:text/html,evil'), null);
  assert.equal(safeUrl('ops/team/messages/_closed-thread/2-closed-reply.md.done'), 'ops/team/messages/_closed-thread/2-closed-reply.md.done');
  const openButton = element('button'); openButton.textContent = messageLabel({ label: 'open thread', closed: false });
  const closedButton = element('button'); closedButton.textContent = messageLabel({ label: 'closed thread', closed: true });
  assert.equal(openButton.textContent, 'open thread');
  assert.equal(closedButton.textContent, 'closed thread (closed)');
});
