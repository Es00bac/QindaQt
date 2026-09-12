import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import test from 'node:test';

import { deriveEvents, discoverHandoffs, dispatchEvents, mergeReviewSnapshots } from './event-dispatcher.mjs';

const candidate = '6f0b77f1aa2aebb5add7098cc3a419b3807b1a48';

test('handoff event bypasses an unrelated warning cooldown and duplicate scan calls no model', async () => {
  const events = deriveEvents({ reviews: [{ candidate, stage: 'queued', reviewResult: 'pending' }] });
  let calls = 0;
  const first = await dispatchEvents({ events, previous: { warningCooldownUntil: 99_999 }, nowMs: 1_000,
    clock: () => 1_000, queue: async () => { calls += 1; } });
  assert.equal(calls, 1);
  assert.equal(first.lastEvidenceLatencyMs, 0);
  await dispatchEvents({ events, previous: first, nowMs: 1_001, queue: async () => { calls += 1; } });
  assert.equal(calls, 1);
});

test('failed queue remains pending and retries on the next scan', async () => {
  const events = deriveEvents({ reviews: [{ candidate, stage: 'queued' }] });
  let calls = 0;
  const failed = await dispatchEvents({ events, previous: {}, nowMs: 2_000, queue: async () => { calls += 1; throw new Error('queue down'); } });
  assert.equal(Object.keys(failed.pending).length, 1);
  assert.match(Object.values(failed.pending)[0].error, /queue down/);
  const retried = await dispatchEvents({ events, previous: failed, nowMs: 2_012, clock: () => 2_012,
    queue: async () => { calls += 1; } });
  assert.equal(calls, 2);
  assert.equal(Object.keys(retried.pending).length, 0);
  assert.equal(retried.lastEvidenceLatencyMs, 12);
  assert.equal(retried.lastQueueDurationMs, 0);
});

test('resolved events are removed from pending state', async () => {
  const events = deriveEvents({ reviews: [{ candidate, stage: 'queued' }] });
  const failed = await dispatchEvents({ events, previous: {}, nowMs: 3_000,
    queue: async () => { throw new Error('queue down'); } });
  const resolved = await dispatchEvents({ events: [], previous: failed, nowMs: 3_010,
    queue: async () => assert.fail('resolved events must not be queued') });
  assert.deepEqual(resolved.pending, {});
});

test('live waiting worker with backlog is actionable unless an assignment already exists', () => {
  const worker = { id: 'small-team-files', status: 'waiting — handed off', updatedAt: '2026-09-12T21:00:00Z',
    processObservation: { processState: 'alive' } };
  const delivery = [{ id: 'desktop-icons', stage: 'partial', outcome: 'Desktop icons' }];
  assert.equal(deriveEvents({ workers: [worker], delivery }).some((event) => event.type === 'available-capacity'), true);
  assert.equal(deriveEvents({ workers: [worker], delivery,
    assignments: [{ workerId: worker.id, state: 'READY', assignedAt: '2026-09-12T21:01:00Z' }] })
    .some((event) => event.type === 'available-capacity'), false);
  assert.equal(deriveEvents({ workers: [worker], delivery,
    assignments: [{ workerId: worker.id, state: 'READY', assignedAt: '2026-09-12T20:59:00Z' }] })
    .some((event) => event.type === 'available-capacity'), true);
});

test('successive worker handoffs and changed review verdicts have distinct stable keys', () => {
  const delivery = [{ id: 'desktop-icons', stage: 'partial' }];
  const first = deriveEvents({ delivery, workers: [{ id: 'files', status: 'waiting — candidate 11111111 handed off',
    updatedAt: '2026-09-12T21:00:00Z', processObservation: { processState: 'alive' } }] })[0];
  const second = deriveEvents({ delivery, workers: [{ id: 'files', status: 'waiting — candidate 22222222 handed off',
    updatedAt: '2026-09-12T21:10:00Z', processObservation: { processState: 'alive' } }] })[0];
  assert.notEqual(first.key, second.key);
  const accept = deriveEvents({ reviews: [{ candidate, reviewResult: 'ACCEPT', reviewer: 'review' }] })[0];
  const reject = deriveEvents({ reviews: [{ candidate, reviewResult: 'REJECT', reviewer: 'review' }] })[0];
  assert.notEqual(accept.key, reject.key);
});

test('multiple new events are delivered in one bounded queue call', async () => {
  const events = deriveEvents({ managerAlive: false, collectorFresh: false });
  let calls = 0;
  const result = await dispatchEvents({ events, managerAlive: true, previous: {}, nowMs: 5_000,
    clock: () => 5_007, queue: async (batch) => { calls += 1; assert.equal(batch.length, 2); } });
  assert.equal(calls, 1);
  assert.equal(Object.keys(result.delivered).length, 2);
  assert.equal(result.lastQueueDurationMs, 7);
  const unchanged = await dispatchEvents({ events, previous: result, nowMs: 5_020,
    queue: async () => assert.fail('unchanged batch must not queue') });
  assert.equal(unchanged.lastQueueDurationMs, 7);
});

test('discovers a clean worker handoff before the manager review ledger contains it', async () => {
  const root = mkdtempSync(path.join(tmpdir(), 'qindaqt-events-'));
  try {
    const worktree = path.join(root, 'worker');
    mkdirSync(path.join(worktree, 'ops/team/messages/small-team-20260912/files'), { recursive: true });
    execFileSync('git', ['init', '-q'], { cwd: worktree });
    execFileSync('git', ['config', 'user.email', 'fixture@example.invalid'], { cwd: worktree });
    execFileSync('git', ['config', 'user.name', 'Fixture'], { cwd: worktree });
    writeFileSync(path.join(worktree, 'product.txt'), 'candidate\n');
    execFileSync('git', ['add', 'product.txt'], { cwd: worktree });
    execFileSync('git', ['commit', '-qm', 'Fixture candidate'], { cwd: worktree });
    const sha = execFileSync('git', ['rev-parse', 'HEAD'], { cwd: worktree, encoding: 'utf8' }).trim();
    writeFileSync(path.join(worktree, 'ops/team/messages/small-team-20260912/files/handoff.md'),
      `# Candidate handoff\n\nCandidate: ${sha}\n`);
    const found = await discoverHandoffs(root, [{ workerId: 'small-team-files', state: 'working',
      worktree: 'worker', dispatch: 'files.md', assignedAt: '2026-09-12T21:00:00Z' }], new Set());
    assert.equal(found[0].candidate, sha);
    assert.equal((await discoverHandoffs(root, [{ workerId: 'small-team-files', state: 'working',
      worktree: 'worker', dispatch: 'files.md' }], new Set([sha]))).length, 0);
  } finally { rmSync(root, { recursive: true, force: true }); }
});

test('discovers a reviewer verdict for a known reviewing candidate before ledger mutation', async () => {
  const root = mkdtempSync(path.join(tmpdir(), 'qindaqt-review-events-'));
  try {
    const worktree = path.join(root, 'rotated-review');
    mkdirSync(path.join(worktree, 'ops/team/messages/small-team-20260912/exact-review-thread'), { recursive: true });
    execFileSync('git', ['init', '-q'], { cwd: worktree });
    execFileSync('git', ['config', 'user.email', 'fixture@example.invalid'], { cwd: worktree });
    execFileSync('git', ['config', 'user.name', 'Fixture'], { cwd: worktree });
    writeFileSync(path.join(worktree, 'product.txt'), 'candidate\n');
    execFileSync('git', ['add', 'product.txt'], { cwd: worktree });
    execFileSync('git', ['commit', '-qm', 'Fixture candidate'], { cwd: worktree });
    const sha = execFileSync('git', ['rev-parse', 'HEAD'], { cwd: worktree, encoding: 'utf8' }).trim();
    writeFileSync(path.join(worktree, 'ops/team/messages/small-team-20260912/exact-review-thread/verdict.md'),
      `# ACCEPT — exact candidate review\n\n- Candidate: \`${sha}\`\n- Result: **ACCEPT**\n`);
    const found = await discoverHandoffs(root, [{ workerId: 'small-team-review', state: 'reviewing',
      worktree: 'rotated-review', dispatch: 'unrelated-name.md', messageThread: 'exact-review-thread',
      assignedAt: '2026-09-12T21:00:00Z' }], new Set([sha]));
    assert.equal(found.length, 1);
    assert.equal(found[0].candidate, sha);
    assert.equal(found[0].reviewResult, 'ACCEPT');
    const events = deriveEvents({ reviews: [{ candidate: sha, stage: 'reviewing' }, ...found] });
    assert.equal(events.filter((event) => event.type === 'review-result').length, 1);
    assert.equal(events.find((event) => event.type === 'review-result').transition, 'accepted');
    let calls = 0;
    const first = await dispatchEvents({ events, previous: {}, nowMs: 6_000, clock: () => 6_004,
      queue: async () => { calls += 1; } });
    await dispatchEvents({ events, previous: first, nowMs: 6_010,
      queue: async () => { calls += 1; } });
    assert.equal(calls, 1);
  } finally { rmSync(root, { recursive: true, force: true }); }
});

test('does not bind a verdict for another candidate to the review worktree HEAD', async () => {
  const root = mkdtempSync(path.join(tmpdir(), 'qindaqt-review-identity-'));
  try {
    const worktree = path.join(root, 'review');
    const thread = path.join(worktree, 'ops/team/messages/small-team-20260912/review');
    mkdirSync(thread, { recursive: true });
    execFileSync('git', ['init', '-q'], { cwd: worktree });
    execFileSync('git', ['config', 'user.email', 'fixture@example.invalid'], { cwd: worktree });
    execFileSync('git', ['config', 'user.name', 'Fixture'], { cwd: worktree });
    writeFileSync(path.join(worktree, 'product.txt'), 'candidate\n');
    execFileSync('git', ['add', 'product.txt'], { cwd: worktree });
    execFileSync('git', ['commit', '-qm', 'Fixture candidate'], { cwd: worktree });
    const head = execFileSync('git', ['rev-parse', 'HEAD'], { cwd: worktree, encoding: 'utf8' }).trim();
    const other = '0123456789abcdef0123456789abcdef01234567';
    writeFileSync(path.join(thread, 'routing-and-verdict.md'),
      `# Queue note\n\nReview worktree HEAD remains ${head}.\n\n# REJECT — another candidate\n\n- Candidate: \`${other}\`\n- Verdict: **REJECT**\n`);
    const found = await discoverHandoffs(root, [{ workerId: 'small-team-review', state: 'reviewing',
      worktree: 'review', messageThread: 'review' }], new Set([head]));
    assert.equal(found.length, 0);
  } finally { rmSync(root, { recursive: true, force: true }); }
});

test('completed one-shot review is a result event, not a failed reviewer', () => {
  const events = deriveEvents({ reviews: [{ candidate, stage: 'reviewing', reviewResult: 'ACCEPT' }],
    workers: [{ id: 'small-team-review', status: 'waiting — ACCEPT published', processObservation: { processState: 'stopped' } }] });
  assert.equal(events.some((event) => event.type === 'review-result'), true);
  assert.equal(events.some((event) => event.type === 'process-failure'), false);
});

test('dead manager and stale collector are degraded signals', () => {
  const events = deriveEvents({ managerAlive: false, collectorFresh: false });
  assert.deepEqual(events.map((event) => event.type).sort(), ['collector-stale', 'manager-dead']);
});

test('out-of-order review snapshots cannot regress an integrated candidate', () => {
  const integrated = mergeReviewSnapshots({}, [{ candidate, stage: 'integrated', integratedCommit: 'abc' }]);
  const late = mergeReviewSnapshots(integrated, [{ candidate, stage: 'reviewing', reviewResult: 'pending' }]);
  assert.equal(late[candidate], 'integrated');
});
