import assert from 'node:assert/strict';
import test from 'node:test';

import { deriveEvents, dispatchEvents, mergeReviewSnapshots } from './event-dispatcher.mjs';

const candidate = '6f0b77f1aa2aebb5add7098cc3a419b3807b1a48';

test('handoff event bypasses an unrelated warning cooldown and duplicate scan calls no model', async () => {
  const events = deriveEvents({ reviews: [{ candidate, stage: 'queued', reviewResult: 'pending' }] });
  let calls = 0;
  const first = await dispatchEvents({ events, previous: { warningCooldownUntil: 99_999 }, nowMs: 1_000, queue: async () => { calls += 1; } });
  assert.equal(calls, 1);
  assert.equal(first.lastLatencyMs, 0);
  await dispatchEvents({ events, previous: first, nowMs: 1_001, queue: async () => { calls += 1; } });
  assert.equal(calls, 1);
});

test('failed queue remains pending and retries on the next scan', async () => {
  const events = deriveEvents({ reviews: [{ candidate, stage: 'queued' }] });
  let calls = 0;
  const failed = await dispatchEvents({ events, previous: {}, nowMs: 2_000, queue: async () => { calls += 1; throw new Error('queue down'); } });
  assert.equal(Object.keys(failed.pending).length, 1);
  assert.match(Object.values(failed.pending)[0].error, /queue down/);
  const retried = await dispatchEvents({ events, previous: failed, nowMs: 2_012, queue: async () => { calls += 1; } });
  assert.equal(calls, 2);
  assert.equal(Object.keys(retried.pending).length, 0);
  assert.equal(retried.lastLatencyMs, 12);
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
  const worker = { id: 'small-team-files', status: 'waiting — handed off', processObservation: { processState: 'alive' } };
  const delivery = [{ id: 'desktop-icons', stage: 'partial', outcome: 'Desktop icons' }];
  assert.equal(deriveEvents({ workers: [worker], delivery }).some((event) => event.type === 'available-capacity'), true);
  assert.equal(deriveEvents({ workers: [worker], delivery, assignments: [{ workerId: worker.id, state: 'READY' }] }).some((event) => event.type === 'available-capacity'), false);
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
