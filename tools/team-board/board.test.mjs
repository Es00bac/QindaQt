import assert from 'node:assert/strict';
import test from 'node:test';
import { mkdtemp, mkdir, writeFile, symlink } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';

import { buildBoard, parseWorkerMarkdown, readBoard } from './board.mjs';
import { createTeamBoardServer, parseServerArguments } from './server.mjs';
import { parseMarkdown, safeMarkdownUrl } from './markdown.mjs';

const worker = (name, status, updatedAt, update, extra = '') => `---
name: ${name}
role: Engineer
provider: Test
model: test-model
reasoning: high
status: ${status}
feature: QQ-001
started_at: 2026-08-26T10:00:00Z
updated_at: ${updatedAt}
${extra}---

# ${name}

## Updates

- ${updatedAt} — ${update}
`;

// Mirrors the QindaQt manager/worker record convention: `# Name` heading,
// wrapped `- Key: value` bullets, free-form status prose, and dated wrapped
// update entries.
const recordWorker = (name, status, updateLines) => `# ${name}

- Provider/model: Test, \`test-model\`, reasoning: high
- Role: Engineer
- Status: ${status}
- Outcome: persistent notification quieting (\`docs/TASK_LIST.md\` active outcome)
- Worktree: \`/tmp/${name.toLowerCase().replaceAll(' ', '-')}\`

## Updates

${updateLines.map((lines) => `- ${lines.join('\n  ')}`).join('\n')}
`;

const evidence = [{ kind: 'test', reference: 'test.mjs', summary: 'Focused test passed.' }];

test('derives product progress only from outcome state plus stopping-point evidence', () => {
  const nowMs = Date.parse('2026-08-26T12:05:00Z');
  const board = buildBoard({
    program: { name: 'Fixture', sourceRows: 7 },
    features: [
      { id: 'QQ-001', state: 'QUALIFIED', stoppingPoint: { evidence } },
      { id: 'QQ-002', state: 'EXECUTABLE', stoppingPoint: { evidence } },
      { id: 'QQ-003', state: 'QUALIFIED', stoppingPoint: { evidence: [] } },
      { id: 'QQ-004', state: 'QUALIFIED', ownerExcluded: true, stoppingPoint: { evidence } },
    ],
  }, [
    parseWorkerMarkdown(worker('Assigned worker', 'assigned', '2026-08-26T12:00:00Z', 'Audit is 95% complete.', 'progress: 95\n')),
    parseWorkerMarkdown(worker('Working worker', 'working', '2026-08-26T12:04:00Z', 'Implementing evidence recorder.')),
  ], { nowMs });

  assert.equal(board.program.qualified, 1);
  assert.equal(board.program.actionableRows, 6);
  assert.equal(board.program.ownerExcludedRows, 1);
  assert.equal(board.program.recordedActionableRows, 3);
  assert.equal(board.program.evidencePoints, 175);
  assert.equal(board.program.evidencePercent, 29.17);
  assert.equal(board.program.evidenceBreadthPoints, 200,
    'qualified and executable rows both count as integrated footprint');
  assert.equal(board.program.evidenceBreadthPercent, 33.33);
  assert.equal(board.features.find((feature) => feature.id === 'QQ-004')?.ownerExcluded, true);
  assert.equal(board.workers[0].name, 'Working worker', 'only declared working status is active and sorts first');
  assert.equal(board.workers[1].active, false, 'assigned status is not fabricated as active work');
  assert.match(board.program.formula, /task percentages add zero/);
});

test('derives a milestone percentage from weighted evidence-backed sub-outcomes', () => {
  const board = buildBoard({
    program: { name: 'Fixture', sourceRows: 1 },
    features: [{
      id: 'QQ-004',
      state: 'EXECUTABLE',
      stoppingPoint: { evidence },
      steps: [
        { id: 'QQ-004.01', title: 'Qualified slice', weight: 30, state: 'QUALIFIED', evidence },
        { id: 'QQ-004.02', title: 'Model only', weight: 50, state: 'MODELLED', evidence },
        { id: 'QQ-004.03', title: 'Unrecorded claim', weight: 20, state: 'QUALIFIED', evidence: [] },
      ],
    }],
  }, []);

  assert.equal(board.features[0].steps[0].contributionPoints, 30);
  assert.equal(board.features[0].steps[1].contributionPoints, 12.5);
  assert.equal(board.features[0].steps[2].contributionPoints, 0,
    'a claimed state without stopping-point evidence contributes nothing');
  assert.equal(board.features[0].evidencePercent, 42.5);
  assert.equal(board.program.evidencePoints, 42.5);
  assert.equal(board.program.evidencePercent, 42.5);
  assert.equal(board.program.evidenceBreadthPoints, 80,
    'recorded qualified/modelled step weight counts once regardless of maturity');
  assert.equal(board.program.evidenceBreadthPercent, 80);
  assert.equal(board.program.qualified, 0);
});

test('fails worker liveness closed for stale, missing, and future declarations', () => {
  const nowMs = Date.parse('2026-08-26T12:30:00Z');
  const board = buildBoard({
    program: { name: 'Fixture', sourceRows: 1 },
    features: [],
  }, [
    parseWorkerMarkdown(worker('Fresh worker', 'working', '2026-08-26T12:29:00Z', 'Running a real gate.')),
    parseWorkerMarkdown(worker('Stale worker', 'working', '2026-08-26T11:59:59Z', 'Old process ended.')),
    parseWorkerMarkdown(worker('Future worker', 'working', '2026-08-26T13:00:00Z', 'Clock is invalid.')),
    parseWorkerMarkdown(worker('Finished worker', 'finished', '2026-08-26T12:29:30Z', 'Handoff complete.')),
  ], { nowMs });

  assert.deepEqual(
    board.workers.filter((entry) => entry.active).map((entry) => entry.name),
    ['Fresh worker'],
  );
  assert.equal(board.workers.find((entry) => entry.name === 'Stale worker')?.active, false);
  assert.equal(board.workers.find((entry) => entry.name === 'Future worker')?.active, false);
  assert.equal(board.workers.find((entry) => entry.name === 'Finished worker')?.active, false);
});

test('parses the QindaQt record convention with wrapped bullets and free-form status', () => {
  const parsed = parseWorkerMarkdown(recordWorker('Ada Ruiz', 'working — repairing exact-review asynchronous completion P2', [
    ['2026-08-27T02:30:03Z — Read the task sources and claimed the complete outcome',
      'from the exact clean base.'],
    ['2026-08-27T02:44:54Z — Schema v2/migration and copy-on-write repository landed'],
  ]));

  assert.equal(parsed.name, 'Ada Ruiz');
  assert.equal(parsed.role, 'Engineer');
  assert.equal(parsed.feature, 'persistent notification quieting (`docs/TASK_LIST.md` active outcome)');
  assert.equal(parsed.status, 'working — repairing exact-review asynchronous completion P2');
  assert.match(parsed.provider, /test-model/);
  assert.equal(parsed.reasoning, 'high');
  assert.equal(parsed.worktree, '/tmp/ada-ruiz');
  assert.equal(parsed.updatedAt, '2026-08-27T02:44:54.000Z');
  assert.equal(parsed.updates.length, 2);
  assert.equal(parsed.updates[0].at, '2026-08-27T02:30:03Z');
  assert.equal(parsed.updates[0].message, 'Read the task sources and claimed the complete outcome from the exact clean base.');
  assert.equal(parsed.updates[1].message, 'Schema v2/migration and copy-on-write repository landed');
  assert.equal(parsed.active, true);

  const paused = parseWorkerMarkdown(recordWorker('Mira Chen', 'paused — provider session limit observed by manager', [
    ['2026-08-27T01:00:00Z — Stopped after the provider session limit'],
  ]));
  assert.equal(paused.active, false, 'free-form non-working status is never active');
  assert.equal(paused.updatedAt, '2026-08-27T01:00:00.000Z', 'newest update entry is the record time');
});

test('reads both live record shapes and exposes plain-English updates', async () => {
  const root = await mkdtemp(path.join(tmpdir(), 'team-board-'));
  await mkdir(path.join(root, 'workers'));
  await writeFile(path.join(root, 'features.json'), JSON.stringify({
    program: { name: 'Fixture', sourceRows: 2 },
    features: [{ id: 'QQ-001', state: 'MODELLED', stoppingPoint: { evidence } }],
  }));
  await writeFile(path.join(root, 'workers', 'avery-ox.md'), worker('Avery Ox', 'assigned', '2026-08-26T14:08:00Z', 'Manager assignment: implement QQ-005.'));
  // Liveness is measured against real wall time, so the working record must
  // carry a deliberately fresh dated update to count as active.
  const freshAt = new Date(Date.now() - 60_000).toISOString();
  await writeFile(path.join(root, 'workers', 'noor-hale.md'), recordWorker('Noor Hale', 'working — repairing exact-review P2', [
    [`${freshAt} — Reproducing the review finding`],
  ]));
  await writeFile(path.join(root, 'workers', 'README.md'), '# Worker records\n');
  const board = await readBoard(root);
  assert.deepEqual(board.workers.map((entry) => entry.name), ['Noor Hale', 'Avery Ox'],
    'the workers README is documentation, never an employee record');
  assert.equal(board.workers[0].name, 'Noor Hale', 'the fresh working record sorts first regardless of format');
  assert.equal(board.workers[0].active, true);
  assert.equal(board.workers[1].name, 'Avery Ox');
  assert.equal(board.workers[1].active, false);
  assert.equal(board.messages[0].at, freshAt);
  assert.equal(board.messages[0].message, 'Reproducing the review finding');
  assert.equal(board.messages[1].at, '2026-08-26T14:08:00Z');
  assert.equal(board.messages[1].message, 'Manager assignment: implement QQ-005.');
});

test('does not let a stale roster hide fresh durable employee records', async () => {
  const root = await mkdtemp(path.join(tmpdir(), 'team-board-roster-'));
  await mkdir(path.join(root, 'workers'));
  await writeFile(path.join(root, 'features.json'), JSON.stringify({
    program: { name: 'Fixture', sourceRows: 1 },
    features: [{ id: 'QQ-001', state: 'QUALIFIED', stoppingPoint: { evidence } }],
  }));
  await writeFile(path.join(root, 'ROSTER.md'), `# Roster

## Current 1 workers

| Employee | Role |
| --- | --- |
| Current Worker | Engineer |

## History
`);
  await writeFile(path.join(root, 'workers', 'current-worker.md'), recordWorker('Current Worker', 'working — focused proof', [
    [new Date().toISOString() + ' — Running the proof'],
  ]));
  await writeFile(path.join(root, 'workers', 'historical-worker.md'), recordWorker('Historical Worker', 'working — stale roster claim', [
    [new Date().toISOString() + ' — This record is not on the current roster'],
  ]));

  const board = await readBoard(root);
  assert.deepEqual(new Set(board.workers.map((entry) => entry.name)),
    new Set(['Current Worker', 'Historical Worker']));
  assert.equal(board.workers.find((entry) => entry.name === 'Current Worker')?.active, true);
  assert.equal(board.workers.find((entry) => entry.name === 'Historical Worker')?.active, true,
    'a genuine fresh worker remains visible even when roster prose lags the claim');
});

test('does not count a working record with missing parser-supported identity fields', () => {
  const parsed = parseWorkerMarkdown(`# Bold Worker

- **Provider/model:** Test
- **Role:** Engineer
- **Status:** working — invisible malformed claim
- **Outcome:** Something

## Updates

- 2026-08-27T02:30:03Z — Running
`, 'bold-worker.md');
  assert.equal(parsed.recordValid, false);
  assert.equal(parsed.active, false);
  assert.match(parsed.recordIssues.join(' '), /role is missing/);
});

test('isolates a malformed employee record without hiding valid workers or outcome progress', async (context) => {
  const root = await mkdtemp(path.join(tmpdir(), 'team-board-malformed-'));
  await mkdir(path.join(root, 'workers'));
  await writeFile(path.join(root, 'features.json'), JSON.stringify({
    program: { name: 'Fixture', sourceRows: 2 },
    features: [{ id: 'QQ-001', state: 'QUALIFIED', stoppingPoint: { evidence } }],
  }));
  await writeFile(path.join(root, 'workers', 'valid-worker.md'), worker('Valid Worker', 'working', new Date().toISOString(), 'Running a real gate.'));
  await writeFile(path.join(root, 'workers', 'broken-worker.md'), '---\nname: Broken Worker\n');

  const board = await readBoard(root);
  assert.equal(board.program.qualified, 1);
  assert.deepEqual(board.workers.map((entry) => entry.name), ['Valid Worker']);
  assert.deepEqual(board.workerErrors, [{
    fileName: 'broken-worker.md',
    message: 'broken-worker.md has unclosed YAML front matter',
  }]);

  const server = createTeamBoardServer({ teamRoot: root });
  context.after(() => server.close());
  await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
  const address = server.address();
  const response = await fetch(`http://127.0.0.1:${address.port}/api/board`);
  assert.equal(response.status, 200);
  const served = await response.json();
  assert.equal(served.program.qualified, 1);
  assert.equal(served.workers.length, 1);
  assert.equal(served.workerErrors[0].fileName, 'broken-worker.md');
});

test('serves a no-store live JSON board on an ephemeral port', async (context) => {
  const server = createTeamBoardServer({ teamRoot: path.resolve('ops/team') });
  context.after(() => server.close());
  await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
  const address = server.address();
  const response = await fetch(`http://127.0.0.1:${address.port}/api/board`);
  assert.equal(response.status, 200);
  assert.equal(response.headers.get('cache-control'), 'no-store');
  const board = await response.json();
  assert.equal(board.program.name, 'QindaQt');
  assert.equal(board.program.sourceRows, 7);
  assert.equal(board.program.actionableRows, board.program.sourceRows - board.program.ownerExcludedRows);
  assert.ok(board.program.qualified >= 0 && board.program.qualified <= board.program.recordedRows);
  assert.equal(
    board.program.evidencePercent,
    Math.round(board.program.evidencePoints / board.program.actionableRows * 100) / 100,
  );
});

test('falls back to the canonical outcome rows when a team root has none', async () => {
  const root = await mkdtemp(path.join(tmpdir(), 'team-board-canonical-'));
  await mkdir(path.join(root, 'workers'));
  await writeFile(path.join(root, 'workers', 'noor-hale.md'), recordWorker('Noor Hale', 'idle', [
    ['2026-08-26T14:09:00Z — Waiting for reassignment'],
  ]));
  const board = await readBoard(root);
  assert.equal(board.program.name, 'QindaQt', 'outcome rows come from the repository record');
  assert.equal(board.program.sourceRows, 7);
  assert.deepEqual(board.workers.map((entry) => entry.name), ['Noor Hale']);
});

test('rejects row ids, states, and server arguments outside the contract', () => {
  assert.throws(() => buildBoard({ program: { name: 'X', sourceRows: 1 }, features: [{ id: 'MH-001', state: 'QUALIFIED', stoppingPoint: { evidence } }] }, [], {}), /QQ source row id/);
  assert.throws(() => buildBoard({ program: { name: 'X', sourceRows: 1 }, features: [{ id: 'QQ-0099', state: 'QUALIFIED', stoppingPoint: { evidence } }] }, [], {}), /QQ source row id/);
  assert.throws(() => buildBoard({ program: { name: 'X', sourceRows: 1 }, features: [{ id: 'QQ-001', state: 'SHIPPED', stoppingPoint: { evidence } }] }, [], {}), /unsupported/);
  assert.throws(() => buildBoard({ program: { name: 'X', sourceRows: 1 }, features: [{ id: 'QQ-001', state: 'MODELLED', stoppingPoint: { evidence }, steps: [{ id: 'QQ-001.01', weight: 90, state: 'MODELLED', evidence }] }] }, [], {}), /weights must total 100/);
  assert.throws(() => buildBoard({ program: { name: 'X', sourceRows: 1 }, features: [{ id: 'QQ-001', state: 'MODELLED', stoppingPoint: { evidence }, steps: [{ id: 'QQ-002.01', weight: 100, state: 'MODELLED', evidence }] }] }, [], {}), /child step id/);
  assert.throws(() => buildBoard({ program: { name: 'X' }, features: [] }, [], {}), /sourceRows/);
  assert.equal(parseServerArguments(['--port', '1234', '--team-root', '/tmp/team']).port, 1234);
  assert.equal(parseServerArguments(['--team-root', 'relative/root']).teamRoot, path.resolve('relative/root'));
  assert.throws(() => parseServerArguments(['--port', '0']), /port/);
  assert.throws(() => parseServerArguments(['--team-root']), /--team-root/);
  assert.throws(() => parseServerArguments(['--wat']), /unknown argument/);
});

test('parses readable Markdown while rejecting dangerous links', () => {
  const document = parseMarkdown('# Title\n\nA **bold** and `code` [safe](https://example.com).\n\n- one\n- two\n\n> quoted\n\n```html\n<script>alert(1)</script>\n```');
  assert.deepEqual(document.blocks.map((block) => block.type), ['heading', 'paragraph', 'list', 'blockquote', 'codeblock']);
  assert.equal(document.blocks[1].children.find((token) => token.type === 'link').url, 'https://example.com');
  assert.equal(safeMarkdownUrl('javascript:alert(1)'), null);
  assert.equal(safeMarkdownUrl('data:text/html,evil'), null);
  assert.equal(safeMarkdownUrl('../secret'), '../secret');
});

test('message API confines traversal and symlink escapes', async (context) => {
  const root = await mkdtemp(path.join(tmpdir(), 'team-board-messages-')); const outside = await mkdtemp(path.join(tmpdir(), 'team-board-outside-'));
  await mkdir(path.join(root, 'workers')); await mkdir(path.join(root, 'messages', 'safe'), { recursive: true });
  await writeFile(path.join(root, 'features.json'), JSON.stringify({ program: { name: 'Fixture', sourceRows: 1 }, features: [] }));
  await writeFile(path.join(root, 'messages', 'safe', '1-reply.md'), '# Safe'); await writeFile(path.join(outside, '1-secret.md'), 'secret');
  await symlink(path.join(outside), path.join(root, 'messages', 'escaped'));
  const server = createTeamBoardServer({ teamRoot: root }); context.after(() => server.close()); await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve)); const address = server.address();
  const listed = await (await fetch(`http://127.0.0.1:${address.port}/api/messages`)).json(); assert.deepEqual(listed.map((t) => t.id), ['safe']);
  assert.equal((await fetch(`http://127.0.0.1:${address.port}/api/messages/safe/..%2F..%2Fsecret.md`)).status, 400);
  assert.equal((await fetch(`http://127.0.0.1:${address.port}/api/messages/escaped/1-secret.md`)).status, 500);
});

test('lists and fetches closed threads and replies with explicit closed labels', async (context) => {
  const root = await mkdtemp(path.join(tmpdir(), 'team-board-closed-')); await mkdir(path.join(root, 'workers')); await mkdir(path.join(root, 'messages', '_closed-thread'), { recursive: true });
  await writeFile(path.join(root, 'features.json'), JSON.stringify({ program: { name: 'Fixture', sourceRows: 1 }, features: [] }));
  await writeFile(path.join(root, 'messages', '_closed-thread', '1-open-reply.md'), '# Open history');
  await writeFile(path.join(root, 'messages', '_closed-thread', '2-closed-reply.md.done'), '# Closed history');
  const server = createTeamBoardServer({ teamRoot: root }); context.after(() => server.close()); await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve)); const address = server.address();
  const listed = await (await fetch(`http://127.0.0.1:${address.port}/api/messages`)).json();
  assert.deepEqual(listed, [{ id: '_closed-thread', closed: true, label: 'closed thread (closed)', replies: [
    { id: '1-open-reply.md', closed: false, label: 'open reply', bytes: 14 },
    { id: '2-closed-reply.md.done', closed: true, label: 'closed reply (closed)', bytes: 16 },
  ] }]);
  const reply = await fetch(`http://127.0.0.1:${address.port}/api/messages/_closed-thread/2-closed-reply.md.done`); assert.equal(reply.status, 200); assert.equal(await reply.text(), '# Closed history');
});
