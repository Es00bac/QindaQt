import assert from 'node:assert/strict';
import test from 'node:test';
import { execFile } from 'node:child_process';
import { chmod, mkdtemp, mkdir, readFile, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { promisify } from 'node:util';

const execFileAsync = promisify(execFile);
const supervisor = path.resolve('.cache/small-team/supervisor.sh');

async function fixture() {
  const root = await mkdtemp(path.join(tmpdir(), 'qindaqt-supervisor-'));
  const state = path.join(root, 'state');
  const board = path.join(root, 'board');
  await mkdir(path.join(board, 'messages', 'supervisor'), { recursive: true });
  const queueLog = path.join(root, 'queue.log');
  const queue = path.join(root, 'queue.sh');
  await writeFile(queue, `#!/usr/bin/env bash\nprintf '%s\\n' "$3" >> '${queueLog}'\nexit "\${QUEUE_EXIT:-0}"\n`);
  await chmod(queue, 0o700);
  return { root, state, board, queue, queueLog };
}

async function tick(fx, { epoch, issues, queueExit = 0 }) {
  await execFileAsync('bash', [supervisor], { env: {
    ...process.env,
    TEAM_ROOT_OVERRIDE: fx.root,
    STATE_ROOT_OVERRIDE: fx.state,
    BOARD_ROOT_OVERRIDE: fx.board,
    SESSION_PATH_OVERRIDE: path.join(fx.root, 'sessions.tsv'),
    SUPERVISOR_ONCE: '1',
    NOW_EPOCH_OVERRIDE: String(epoch),
    NOW_ISO_OVERRIDE: new Date(epoch * 1000).toISOString(),
    COOLDOWN_SECONDS_OVERRIDE: '1200',
    INTERVAL_SECONDS_OVERRIDE: '300',
    ISSUES_OVERRIDE: issues,
    QUEUE_COMMAND_OVERRIDE: fx.queue,
    QUEUE_EXIT: String(queueExit),
  } });
  return JSON.parse(await readFile(path.join(fx.board, 'supervision.json'), 'utf8'));
}

test('clear and duplicate ticks spend no duplicate manager wake', async () => {
  const fx = await fixture();
  let result = await tick(fx, { epoch: 2_000, issues: '' });
  assert.equal(result.notificationState, 'resolved');
  result = await tick(fx, { epoch: 4_000, issues: 'dock: candidate ready; ' });
  assert.equal(result.notificationState, 'notified');
  result = await tick(fx, { epoch: 4_100, issues: 'dock: candidate ready; ' });
  assert.equal(result.notificationState, 'notified');
  await writeFile(path.join(fx.state, 'acknowledged-key'), result.lastNotifiedKey);
  result = await tick(fx, { epoch: 4_101, issues: 'dock: candidate ready; ' });
  assert.equal(result.notificationState, 'acknowledged');
  assert.equal((await readFile(fx.queueLog, 'utf8')).trim().split('\n').length, 1);
});

test('new issue during cooldown remains pending and is eventually delivered', async () => {
  const fx = await fixture();
  await tick(fx, { epoch: 4_000, issues: 'dock: candidate ready; ' });
  let result = await tick(fx, { epoch: 4_100, issues: 'files: candidate awaiting review; ' });
  assert.equal(result.notificationState, 'pending');
  assert.ok(result.pendingKey);
  assert.equal((await readFile(fx.queueLog, 'utf8')).trim().split('\n').length, 1);
  result = await tick(fx, { epoch: 5_201, issues: 'files: candidate awaiting review; ' });
  assert.equal(result.notificationState, 'notified');
  assert.equal(result.pendingKey, '');
  assert.equal((await readFile(fx.queueLog, 'utf8')).trim().split('\n').length, 2);
});

test('failed delivery is truthful and retained for retry', async () => {
  const fx = await fixture();
  let result = await tick(fx, { epoch: 4_000, issues: 'review: candidate pending while reviewer harness absent; ', queueExit: 7 });
  assert.equal(result.health, 'degraded');
  assert.equal(result.notificationState, 'delivery-failed');
  assert.match(result.queueError, /exited 7/);
  assert.ok(result.pendingKey);
  result = await tick(fx, { epoch: 4_001, issues: 'review: candidate pending while reviewer harness absent; ' });
  assert.equal(result.notificationState, 'notified');
  assert.equal(result.pendingKey, '');
});

test('manager acknowledgment clears a cooldown-pending duplicate', async () => {
  const fx = await fixture();
  await mkdir(fx.state, { recursive: true });
  await writeFile(path.join(fx.state, 'last-wake-epoch'), '4000\n');
  let result = await tick(fx, { epoch: 4_100, issues: 'records: repair at next safe boundary; ' });
  assert.equal(result.notificationState, 'pending');
  await writeFile(path.join(fx.state, 'acknowledged-key'), `${result.detectedKey}\n`);
  result = await tick(fx, { epoch: 4_101, issues: 'records: repair at next safe boundary; ' });
  assert.equal(result.notificationState, 'acknowledged');
  assert.equal(result.pendingKey, '');
});

test('supervisor copies the assignment-mapped worktree and explicit message thread', async () => {
  const fx = await fixture();
  const rotated = path.join(fx.root, 'rotated-files-review');
  await mkdir(path.join(rotated, 'ops/team/workers'), { recursive: true });
  await mkdir(path.join(rotated, 'ops/team/messages/small-team-20260912/exact-thread'), { recursive: true });
  await mkdir(path.join(fx.board, 'workers'), { recursive: true });
  await writeFile(path.join(rotated, 'ops/team/workers/small-team-files.md'), '# mapped worker record\n');
  await writeFile(path.join(rotated, 'ops/team/messages/small-team-20260912/exact-thread/verdict.md'), '# mapped verdict\n');
  await writeFile(path.join(fx.board, 'assignments.json'), JSON.stringify({ assignments: [{
    workerId: 'small-team-files', state: 'reviewing', worktree: 'rotated-files-review',
    messageThread: 'exact-thread',
  }] }));
  await writeFile(path.join(fx.root, 'sessions.tsv'), '');
  await tick(fx, { epoch: 4_000, issues: '' });
  assert.equal(await readFile(path.join(fx.board, 'workers/small-team-files.md'), 'utf8'), '# mapped worker record\n');
  assert.equal(await readFile(path.join(fx.board, 'messages/files/verdict.md'), 'utf8'), '# mapped verdict\n');
});
