import { execFile } from 'node:child_process';
import { readFile, readdir, rename, stat, writeFile } from 'node:fs/promises';
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
  return [event.type, event.identity, event.candidate || event.outcome || 'none', event.transition || 'once'].join(':');
}

function completedTurn(worker) {
  const status = value(worker?.status);
  const candidate = status.match(/\b[0-9a-f]{8,40}\b/i)?.[0];
  return candidate || value(worker?.updatedAt, 'unknown-turn');
}

function assignmentIsNewer(assignment, worker) {
  const assignedAt = Date.parse(assignment?.assignedAt ?? '');
  const updatedAt = Date.parse(worker?.updatedAt ?? '');
  return Number.isFinite(assignedAt) && Number.isFinite(updatedAt) && assignedAt > updatedAt;
}

export function deriveEvents({ reviews = [], workers = [], assignments = [], delivery = [], managerAlive = true, collectorFresh = true } = {}) {
  const events = [];
  const openBacklog = delivery.filter((item) => !/integrated|adopted/i.test(value(item?.stage)));
  for (const review of reviews) {
    const candidate = value(review?.candidate);
    const stage = reviewStage(review);
    if (!candidate || stage === 'integrated') continue;
    if (stage === 'accepted' || stage === 'rejected') {
      events.push({ type: 'review-result', identity: value(review?.reviewer, 'review'), candidate, transition: stage,
        evidenceAt: value(review?.reviewedAt, value(review?.reviewStartedAt, value(review?.queuedAt))),
        message: `${stage.toUpperCase()} received for exact candidate ${candidate}; synchronize integration and next dispatch now.` });
    } else if (stage === 'queued') {
      events.push({ type: 'candidate-ready', identity: value(review?.implementer, 'implementer'), candidate,
        transition: value(review?.turnIdentity, candidate), evidenceAt: value(review?.evidenceAt, value(review?.queuedAt)),
        message: `Exact candidate ${candidate} is queued without an active review transition; route an independent reviewer now.` });
    }
  }
  for (const worker of workers) {
    const observation = worker?.processObservation;
    const workerId = value(worker?.id);
    const status = value(worker?.status).toLowerCase();
    const assignment = assignments.find((item) => value(item?.workerId) === workerId
      && ['ready', 'working', 'reviewing'].includes(value(item?.state).toLowerCase()));
    const hasNewAssignment = assignment && (/^working\b/.test(status) || assignmentIsNewer(assignment, worker));
    if (observation?.processState === 'stopped' && /^working\b/.test(status)) {
      events.push({ type: 'process-failure', identity: workerId,
        message: `${workerId} declares working but its harness is stopped; preserve work and route recovery.` });
    }
    if (observation?.processState === 'alive' && /^waiting\b/.test(status)
        && !hasNewAssignment && openBacklog.length) {
      const item = openBacklog[0];
      events.push({ type: 'available-capacity', identity: workerId, outcome: value(item?.id, value(item?.outcome)),
        transition: completedTurn(worker), evidenceAt: value(worker?.updatedAt),
        message: `${workerId} is live but waiting with backlog available; assign one exact non-conflicting outcome.` });
    }
  }
  if (!managerAlive) events.push({ type: 'manager-dead', identity: 'program-manager', message: 'Program manager process is absent; event delivery is degraded.' });
  if (!collectorFresh) events.push({ type: 'collector-stale', identity: 'activity-collector', message: 'Activity collector is stale; process observations must not be treated as current.' });
  return events.map((event) => Object.freeze({ ...event, key: eventKey(event) }));
}

export async function dispatchEvents({ events, previous = {}, queue, nowMs = Date.now(), clock = Date.now, managerAlive = true }) {
  const delivered = { ...(previous.delivered ?? {}) };
  const pending = { ...(previous.pending ?? {}) };
  const activeKeys = new Set(events.map((event) => event.key));
  for (const key of Object.keys(pending)) {
    if (!activeKeys.has(key)) delete pending[key];
  }
  const deliveredValues = Object.values(delivered);
  let lastEvidenceLatencyMs = Number(previous.lastEvidenceLatencyMs)
    || Math.max(0, ...deliveredValues.map((entry) => Number(entry?.evidenceLatencyMs) || 0));
  let lastQueueDurationMs = Number(previous.lastQueueDurationMs)
    || Math.max(0, ...deliveredValues.map((entry) => Number(entry?.queueDurationMs) || 0));
  const batch = [];
  for (const event of events) {
    if (delivered[event.key]) { delete pending[event.key]; continue; }
    const evidenceAt = Date.parse(event.evidenceAt ?? '');
    const firstSeenAt = pending[event.key]?.firstSeenAt ?? (Number.isFinite(evidenceAt) ? evidenceAt : nowMs);
    pending[event.key] = { event, firstSeenAt, lastAttemptAt: nowMs, error: '' };
    batch.push(event);
  }
  if (managerAlive && batch.length) {
    try {
      await queue(batch);
      const deliveredAt = clock();
      lastQueueDurationMs = Math.max(0, deliveredAt - nowMs);
      for (const event of batch) {
        const firstSeenAt = pending[event.key].firstSeenAt;
        const evidenceLatencyMs = Math.max(0, deliveredAt - firstSeenAt);
        lastEvidenceLatencyMs = evidenceLatencyMs;
        delivered[event.key] = { event, firstSeenAt, deliveredAt, evidenceLatencyMs,
          queueDurationMs: lastQueueDurationMs };
        delete pending[event.key];
      }
    } catch (error) {
      for (const event of batch) pending[event.key].error = error instanceof Error ? error.message : String(error);
    }
  }
  return { delivered, pending, lastEvidenceLatencyMs, lastQueueDurationMs };
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

async function candidateEvidence(worktree, assignment, candidate) {
  const lane = value(assignment?.workerId).replace(/^small-team-/, '');
  const dispatch = path.basename(value(assignment?.dispatch), '.md');
  const roots = [...new Set([lane, dispatch].filter(Boolean))]
    .map((name) => path.join(worktree, 'ops/team/messages/small-team-20260912', name));
  const files = [];
  for (const root of roots) {
    try {
      for (const entry of await readdir(root, { withFileTypes: true })) {
        if (entry.isFile() && entry.name.endsWith('.md')) files.push(path.join(root, entry.name));
      }
    } catch (error) { if (error?.code !== 'ENOENT') throw error; }
  }
  const record = path.join(worktree, 'ops/team/workers', `${value(assignment?.workerId)}.md`);
  try { if ((await stat(record)).isFile()) files.push(record); } catch (error) { if (error?.code !== 'ENOENT') throw error; }
  for (const file of files) {
    const content = await readFile(file, 'utf8');
    if (content.includes(candidate) && /\b(candidate|handoff)\b/i.test(content)) {
      return { file, evidenceAt: (await stat(file)).mtime.toISOString() };
    }
  }
  return null;
}

export async function discoverHandoffs(teamRoot, assignments = [], knownCandidates = new Set()) {
  const handoffs = [];
  for (const assignment of assignments) {
    if (!['working', 'candidate', 'ready'].includes(value(assignment?.state).toLowerCase())) continue;
    const worktree = path.resolve(teamRoot, value(assignment?.worktree));
    if (worktree !== path.resolve(teamRoot) && !worktree.startsWith(`${path.resolve(teamRoot)}${path.sep}`)) continue;
    let candidate;
    try {
      candidate = value((await execFileAsync('git', ['rev-parse', 'HEAD'], { cwd: worktree })).stdout);
      if (!candidate || knownCandidates.has(candidate)) continue;
      if (value((await execFileAsync('git', ['status', '--porcelain', '--untracked-files=no'], { cwd: worktree })).stdout)) continue;
    } catch { continue; }
    const evidence = await candidateEvidence(worktree, assignment, candidate);
    if (!evidence) continue;
    handoffs.push({ candidate, implementer: value(assignment.workerId), stage: 'queued', reviewResult: 'pending',
      evidenceAt: evidence.evidenceAt, queuedAt: evidence.evidenceAt,
      turnIdentity: `${value(assignment.assignedAt, evidence.evidenceAt)}:${candidate}` });
  }
  return handoffs;
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
  const discovered = await discoverHandoffs(teamRoot, assignments.assignments,
    new Set(board.reviews.map((review) => value(review.candidate))));
  const allReviews = [...board.reviews, ...discovered];
  const reviewStages = mergeReviewSnapshots(previous.reviewStages, allReviews);
  const effectiveReviews = allReviews.map((review) => {
    const effective = reviewStages[review.candidate];
    return effective === 'integrated' ? { ...review, stage: 'integrated' } : review;
  });
  const events = deriveEvents({ reviews: effectiveReviews, workers: board.workers,
    assignments: assignments.assignments, delivery: board.delivery, managerAlive, collectorFresh });
  const queue = async (batch) => {
    const details = batch.map((event) => `${event.key}: ${event.message}`).join(' | ');
    const message = `Immediate delivery events: ${details} These product events bypass warning cooldown; inspect exact evidence and update routing atomically.`;
    if (queueCommand) await queueCommand(managerThread, message, batch);
    else await execFileAsync('codex', ['queue', '--thread', managerThread, '--message', message]);
  };
  const transition = await dispatchEvents({ events, previous, queue, nowMs, managerAlive });
  const pendingValues = Object.values(transition.pending);
  const health = managerAlive && collectorFresh && pendingValues.every((entry) => !entry.error) ? 'healthy' : 'degraded';
  const result = {
    lastScan: new Date(nowMs).toISOString(), lastSuccessfulScan: new Date(nowMs).toISOString(),
    nextScan: new Date(nowMs + 10_000).toISOString(), health, managerAlive, collectorFresh,
    scanIntervalMs: 10_000, warningCooldownApplies: false,
    lastEvidenceLatencyMs: transition.lastEvidenceLatencyMs,
    lastQueueDurationMs: transition.lastQueueDurationMs,
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
