import { execFile } from 'node:child_process';
import { readFile, rename, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { promisify } from 'node:util';

import { readBoard } from './board.mjs';

const execFileAsync = promisify(execFile);
const REVIEW_RANK = Object.freeze({ queued: 1, reviewing: 2, accepted: 3, rejected: 3, integrated: 4 });

function value(text, fallback = '') { return typeof text === 'string' && text.trim() ? text.trim() : fallback; }
function reviewStage(review) {
  const result = value(review?.reviewResult).toLowerCase();
  const stage = value(review?.stage).toLowerCase();
  if (stage === 'integrated' || review?.integratedCommit) return 'integrated';
  if (result === 'accept') return 'accepted';
  if (result && result !== 'pending') return 'rejected';
  return stage === 'reviewing' ? 'reviewing' : 'queued';
}

export function mergeReviewSnapshots(previous = {}, reviews = []) {
  const merged = { ...previous };
  for (const review of reviews) {
    const candidate = value(review?.candidate);
    if (!candidate) continue;
    const next = reviewStage(review);
    const prior = value(merged[candidate]);
    merged[candidate] = (REVIEW_RANK[prior] ?? 0) > (REVIEW_RANK[next] ?? 0) ? prior : next;
  }
  return merged;
}

export function eventKey(event) {
  return [event.type, event.identity, event.candidate || event.outcome || 'none'].join(':');
}

export function deriveEvents({ reviews = [], workers = [], assignments = [], delivery = [], managerAlive = true, collectorFresh = true } = {}) {
  const events = [];
  const activeAssignments = new Set(assignments
    .filter((assignment) => ['ready', 'working', 'reviewing'].includes(value(assignment?.state).toLowerCase()))
    .map((assignment) => value(assignment?.workerId)));
  const openBacklog = delivery.filter((item) => !/integrated|adopted/i.test(value(item?.stage)));
  for (const review of reviews) {
    const candidate = value(review?.candidate);
    const stage = reviewStage(review);
    if (!candidate || stage === 'integrated') continue;
    if (stage === 'accepted' || stage === 'rejected') {
      events.push({ type: 'review-result', identity: value(review?.reviewer, 'review'), candidate,
        message: `${stage.toUpperCase()} received for exact candidate ${candidate}; synchronize integration and next dispatch now.` });
    } else if (stage === 'queued') {
      events.push({ type: 'candidate-ready', identity: value(review?.implementer, 'implementer'), candidate,
        message: `Exact candidate ${candidate} is queued without an active review transition; route an independent reviewer now.` });
    }
  }
  for (const worker of workers) {
    const observation = worker?.processObservation;
    const workerId = value(worker?.id);
    const status = value(worker?.status).toLowerCase();
    if (observation?.processState === 'stopped' && /^working\b/.test(status)) {
      events.push({ type: 'process-failure', identity: workerId,
        message: `${workerId} declares working but its harness is stopped; preserve work and route recovery.` });
    }
    if (observation?.processState === 'alive' && /^waiting\b/.test(status)
        && !activeAssignments.has(workerId) && openBacklog.length) {
      const item = openBacklog[0];
      events.push({ type: 'available-capacity', identity: workerId, outcome: value(item?.id, value(item?.outcome)),
        message: `${workerId} is live but waiting with backlog available; assign one exact non-conflicting outcome.` });
    }
  }
  if (!managerAlive) events.push({ type: 'manager-dead', identity: 'program-manager', message: 'Program manager process is absent; event delivery is degraded.' });
  if (!collectorFresh) events.push({ type: 'collector-stale', identity: 'activity-collector', message: 'Activity collector is stale; process observations must not be treated as current.' });
  return events.map((event) => Object.freeze({ ...event, key: eventKey(event) }));
}

export async function dispatchEvents({ events, previous = {}, queue, nowMs = Date.now(), managerAlive = true }) {
  const delivered = { ...(previous.delivered ?? {}) };
  const pending = { ...(previous.pending ?? {}) };
  const activeKeys = new Set(events.map((event) => event.key));
  for (const key of Object.keys(pending)) {
    if (!activeKeys.has(key)) delete pending[key];
  }
  let lastLatencyMs = Number(previous.lastLatencyMs) || 0;
  for (const event of events) {
    if (delivered[event.key]) { delete pending[event.key]; continue; }
    const firstSeenAt = pending[event.key]?.firstSeenAt ?? nowMs;
    pending[event.key] = { event, firstSeenAt, lastAttemptAt: nowMs, error: '' };
    if (!managerAlive) continue;
    try {
      await queue(event);
      const deliveredAt = nowMs;
      lastLatencyMs = Math.max(0, deliveredAt - firstSeenAt);
      delivered[event.key] = { event, firstSeenAt, deliveredAt, latencyMs: lastLatencyMs };
      delete pending[event.key];
    } catch (error) {
      pending[event.key].error = error instanceof Error ? error.message : String(error);
    }
  }
  return { delivered, pending, lastLatencyMs };
}

async function processMatches(pid, expected) {
  try { return (await readFile(`/proc/${pid}/comm`, 'utf8')).trim() === expected; } catch { return false; }
}

async function readJson(file, fallback) {
  try { return JSON.parse(await readFile(file, 'utf8')); } catch (error) { if (error?.code === 'ENOENT') return fallback; throw error; }
}

async function atomicJson(file, payload) {
  const temporary = `${file}.${process.pid}.tmp`;
  await writeFile(temporary, `${JSON.stringify(payload, null, 2)}\n`, { mode: 0o600 });
  await rename(temporary, file);
}

export async function runEventScan({ teamRoot, boardRoot, stateFile, managerPid, managerThread, collectorMaxAgeMs = 30_000, queueCommand } = {}) {
  const nowMs = Date.now();
  const [board, assignments, previous] = await Promise.all([
    readBoard(boardRoot),
    readJson(path.join(boardRoot, 'assignments.json'), { assignments: [] }),
    readJson(stateFile, {}),
  ]);
  const managerAlive = await processMatches(managerPid, 'codex');
  const collectedAt = Date.parse(board.activityHealth?.collectedAt ?? '');
  const collectorFresh = Number.isFinite(collectedAt) && nowMs - collectedAt <= collectorMaxAgeMs;
  const reviewStages = mergeReviewSnapshots(previous.reviewStages, board.reviews);
  const effectiveReviews = board.reviews.map((review) => {
    const effective = reviewStages[review.candidate];
    return effective === 'integrated' ? { ...review, stage: 'integrated' } : review;
  });
  const events = deriveEvents({ reviews: effectiveReviews, workers: board.workers,
    assignments: assignments.assignments, delivery: board.delivery, managerAlive, collectorFresh });
  const queue = async (event) => {
    const message = `Immediate delivery event ${event.key}: ${event.message} This product event bypasses warning cooldown; inspect exact evidence and update routing atomically.`;
    if (queueCommand) await queueCommand(managerThread, message, event);
    else await execFileAsync('codex', ['queue', '--thread', managerThread, '--message', message]);
  };
  const transition = await dispatchEvents({ events, previous, queue, nowMs, managerAlive });
  const pendingValues = Object.values(transition.pending);
  const health = managerAlive && collectorFresh && pendingValues.every((entry) => !entry.error) ? 'healthy' : 'degraded';
  const result = {
    lastScan: new Date(nowMs).toISOString(), lastSuccessfulScan: new Date(nowMs).toISOString(),
    nextScan: new Date(nowMs + 10_000).toISOString(), health, managerAlive, collectorFresh,
    scanIntervalMs: 10_000, warningCooldownApplies: false, lastLatencyMs: transition.lastLatencyMs,
    activeEvents: events, delivered: transition.delivered, pending: transition.pending, reviewStages,
  };
  await atomicJson(stateFile, result);
  return result;
}

if (process.argv[1] === new URL(import.meta.url).pathname) {
  const teamRoot = process.env.TEAM_ROOT ?? path.resolve('.');
  const boardRoot = process.env.BOARD_ROOT ?? path.join(teamRoot, '.cache/small-team/dashboard');
  const stateFile = process.env.EVENT_STATE ?? path.join(boardRoot, 'event-dispatch.json');
  const managerPid = Number(process.env.MANAGER_PID ?? 0);
  const managerThread = process.env.MANAGER_THREAD ?? '';
  await runEventScan({ teamRoot, boardRoot, stateFile, managerPid, managerThread });
}
