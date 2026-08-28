import { readFile, readdir } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

// AGENT-CONTRACT: progress is derived only from recorded outcome rows in
// ops/team/features.json plus declared worker status. Assignments, task
// percentages, process liveness, and message activity must never contribute.
export const FEATURE_STATE_WEIGHTS = Object.freeze({
  UNVERIFIED: 0,
  ABSENT: 0,
  MODELLED: 25,
  WIRED: 50,
  EXECUTABLE: 75,
  QUALIFIED: 100,
});

export const WORKER_ACTIVITY_MAX_AGE_MS = 30 * 60 * 1_000;
export const WORKER_CLOCK_SKEW_MS = 5 * 60 * 1_000;

// AGENT-GUARD: this pattern must stay in lockstep with the row ids written in
// ops/team/features.json; widening it silently lets arbitrary progress rows
// into the denominator of the project evidence percentage.
const FEATURE_ID_PATTERN = /^QQ-\d{3}$/;
const STEP_ID_PATTERN = /^QQ-\d{3}\.\d{2}$/;
const PROVIDER_ID_PATTERN = /^[a-z][a-z0-9-]*$/;
const PROVIDER_STATES = new Set(['available', 'degraded', 'unavailable']);

function text(value, fallback = '') {
  return typeof value === 'string' && value.trim() ? value.trim() : fallback;
}

// Worker profiles use a deliberately small scalar-only YAML subset. Quoted
// scalars remain valid profile data, so normalize their outer quotes before
// comparing status or parsing timestamps; otherwise a formatting-only change
// can incorrectly remove a live worker from the board.
function frontMatterText(value, fallback = '') {
  const normalized = text(value, fallback);
  if (normalized.length < 2) return normalized;
  if (normalized.startsWith('"') && normalized.endsWith('"')) {
    try { return JSON.parse(normalized); } catch { return normalized; }
  }
  if (normalized.startsWith("'") && normalized.endsWith("'")) {
    return normalized.slice(1, -1).replaceAll("''", "'");
  }
  return normalized;
}

function timestamp(value) {
  const parsed = Date.parse(text(value));
  return Number.isFinite(parsed) ? parsed : 0;
}

function plainText(value) {
  return text(value).replace(/[<>]/g, '').slice(0, 2_000);
}

function normalizeWorkerError(fileName, error) {
  return Object.freeze({
    fileName: path.basename(fileName),
    message: plainText(error instanceof Error ? error.message : String(error), 'Worker record could not be read'),
  });
}

function normalizeProvider(raw, index) {
  const id = text(raw?.id).toLowerCase();
  if (!PROVIDER_ID_PATTERN.test(id)) {
    throw new TypeError(`providers[${index}].id must be a stable lowercase identifier`);
  }
  const status = text(raw?.status).toLowerCase();
  if (!PROVIDER_STATES.has(status)) {
    throw new TypeError(`providers[${index}].status must be available, degraded, or unavailable`);
  }
  const updatedAt = text(raw?.updatedAt);
  if (!timestamp(updatedAt)) throw new TypeError(`providers[${index}].updatedAt must be an ISO timestamp`);
  const estimatedReturnAt = text(raw?.estimatedReturnAt);
  if (estimatedReturnAt && !timestamp(estimatedReturnAt)) {
    throw new TypeError(`providers[${index}].estimatedReturnAt must be empty or an ISO timestamp`);
  }
  return Object.freeze({
    id,
    name: text(raw?.name, id),
    status,
    available: status === 'available',
    updatedAt,
    estimatedReturnAt,
    evidence: plainText(raw?.evidence, 'No capacity evidence recorded'),
  });
}

// Updates are dated plain-English entries; the leading ISO timestamp is the
// entry's own declared time and is the only truthful liveness signal.
const UPDATE_TIME_PATTERN = /^(\d{4}-\d{2}-\d{2}T[^\s]+)/;

function parseUpdateEntry(raw) {
  const match = UPDATE_TIME_PATTERN.exec(raw);
  if (!match || !Number.isFinite(Date.parse(match[1]))) {
    return { at: '', message: raw };
  }
  return { at: match[1], message: raw.slice(match[1].length).replace(/^[\s—–-]+/, '') };
}

function parseFrontMatterFields(source, fileName) {
  const end = source.indexOf('\n---', 4);
  if (end < 0) throw new TypeError(`${fileName} has unclosed YAML front matter`);
  const fields = {};
  for (const line of source.slice(4, end).split('\n')) {
    const separator = line.indexOf(':');
    if (separator < 1) continue;
    fields[line.slice(0, separator).trim()] = line.slice(separator + 1).trim();
  }
  return fields;
}

// AGENT-NOTE: worker records exist in two stable shapes. Flow-style records
// start with YAML front matter; the QindaQt manager/worker convention (see
// ops/team/workers/README.md) uses a `# Name` heading with `- Key: value`
// bullets, free-form status prose, and dated `## Updates` entries whose lines
// wrap. Both shapes must keep parsing; the updates list is the worker's
// message feed and its newest entry timestamp is the record's liveness time.
function parseRecordFields(source) {
  const lines = source.replace(/\r\n?/g, '\n').split('\n');
  let name = '';
  let section = '';
  const fields = {};
  const updateLines = [];
  let current = null;
  const closeCurrent = () => {
    if (!current) return;
    if (current.section === 'updates') updateLines.push(current.text);
    // Records quote paths and identifiers in Markdown code spans; strip a
    // whole-value span so parsed fields compare naturally.
    else if (current.key) fields[current.key] = current.text.replace(/^`(.*)`$/s, '$1');
    current = null;
  };
  for (const line of lines) {
    if (!line.trim()) { closeCurrent(); continue; }
    const heading = line.match(/^#{1,6}\s+(.+?)\s*#*\s*$/);
    if (heading) {
      closeCurrent();
      if (/^#{1}\s/.test(line) && !name) name = heading[1];
      else section = heading[1].toLowerCase();
      continue;
    }
    const bullet = line.match(/^\s*-\s+(.*)$/);
    if (bullet) {
      closeCurrent();
      const entry = bullet[1].trim();
      if (section === 'updates') {
        current = { section, key: '', text: entry };
      } else {
        const separator = entry.indexOf(':');
        if (separator > 0) {
          current = {
            section,
            key: entry.slice(0, separator).trim().toLowerCase(),
            text: entry.slice(separator + 1).trim(),
          };
        }
      }
      continue;
    }
    if (current) current.text = `${current.text} ${line.trim()}`;
  }
  closeCurrent();
  return { name, fields, updateLines };
}

function buildWorkerRecord({ id, name, role, provider, model, reasoning, status, feature, startedAt, updatedAt, worktree, updates, fileName }) {
  const recordIssues = [];
  if (!name || name === id) recordIssues.push('name is missing');
  if (!role || role === 'Unspecified role') recordIssues.push('role is missing');
  if (!provider || provider === 'Unspecified provider') recordIssues.push('provider/model is missing');
  if (!status || status === 'unknown') recordIssues.push('status is missing');
  if (!feature || feature === 'No outcome declared') recordIssues.push('outcome is missing');
  if (!updates.some((update) => timestamp(update.at) > 0)) recordIssues.push('no ISO-dated update exists under ## Updates');
  return Object.freeze({
    id,
    name,
    role,
    provider,
    model,
    reasoning,
    status,
    feature,
    startedAt,
    updatedAt,
    worktree,
    recordValid: recordIssues.length === 0,
    recordIssues: Object.freeze(recordIssues),
    active: recordIssues.length === 0 && /^working\b/i.test(status),
    updates: Object.freeze(updates.map((update) => Object.freeze(update))),
  });
}

export function parseWorkerMarkdown(source, fileName = 'worker.md') {
  const baseName = path.basename(fileName, '.md');
  if (source.startsWith('---\n')) {
    const fields = parseFrontMatterFields(source, fileName);
    const body = source.slice(source.indexOf('\n---', 4) + 4);
    const updatesSection = body.split(/^## Updates\s*$/m)[1] ?? '';
    const updates = updatesSection.split('\n')
      .filter((line) => /^\s*-\s+/.test(line))
      .map((line) => parseUpdateEntry(plainText(line.replace(/^\s*-\s+/, ''))))
      .filter((update) => update.message);
    const newestDeclared = updates.map((update) => timestamp(update.at)).reduce((left, right) => Math.max(left, right), 0);
    return buildWorkerRecord({
      id: baseName,
      name: frontMatterText(fields.name, baseName),
      role: frontMatterText(fields.role, 'Unspecified role'),
      provider: frontMatterText(fields.provider, 'Unspecified provider'),
      model: frontMatterText(fields.model, 'Unspecified model'),
      reasoning: frontMatterText(fields.reasoning, 'Unspecified'),
      status: frontMatterText(fields.status, 'unknown').toLowerCase(),
      feature: frontMatterText(fields.feature, 'No outcome declared'),
      startedAt: frontMatterText(fields.started_at),
      updatedAt: frontMatterText(fields.updated_at) || (newestDeclared ? new Date(newestDeclared).toISOString() : ''),
      worktree: frontMatterText(fields.worktree),
      updates,
      fileName,
    });
  }
  const { name, fields, updateLines } = parseRecordFields(source);
  const updates = updateLines
    .map((line) => parseUpdateEntry(plainText(line)))
    .filter((update) => update.message);
  const newestDeclared = updates.map((update) => timestamp(update.at)).reduce((left, right) => Math.max(left, right), 0);
  const providerLine = text(fields['provider/model'], text(fields.provider, 'Unspecified provider'));
  const reasoningMatch = /reasoning:\s*([^,)]+)/i.exec(providerLine);
  return buildWorkerRecord({
    id: baseName,
    name: text(name, baseName),
    role: text(fields.role, 'Unspecified role'),
    provider: providerLine,
    model: text(fields.model, 'Unspecified model'),
    reasoning: text(fields.reasoning, reasoningMatch ? reasoningMatch[1].trim() : 'Unspecified'),
    status: text(fields.status, 'unknown'),
    feature: text(fields.outcome, text(fields.feature, 'No outcome declared')),
    startedAt: text(fields.started_at ?? fields.started),
    updatedAt: text(fields.updated_at) || (newestDeclared ? new Date(newestDeclared).toISOString() : ''),
    worktree: text(fields.worktree, text(fields['product worktree'])),
    updates,
    fileName,
  });
}

function normalizeEvidence(rawEvidence) {
  return Array.isArray(rawEvidence)
    ? rawEvidence.map((entry) => Object.freeze({
      kind: text(entry?.kind, 'evidence'),
      reference: text(entry?.reference),
      summary: plainText(entry?.summary),
    })) : [];
}

function normalizeStep(raw, index, featureId) {
  const id = text(raw?.id);
  if (!STEP_ID_PATTERN.test(id) || !id.startsWith(`${featureId}.`)) {
    throw new TypeError(`${featureId}.steps[${index}].id must be a child step id such as ${featureId}.01`);
  }
  const weight = Number(raw?.weight);
  if (!Number.isFinite(weight) || weight <= 0 || weight > 100) {
    throw new TypeError(`${id}.weight must be greater than 0 and at most 100`);
  }
  const state = text(raw?.state, 'UNVERIFIED').toUpperCase();
  if (!(state in FEATURE_STATE_WEIGHTS)) throw new TypeError(`${id}.state is unsupported`);
  const evidence = normalizeEvidence(raw?.evidence);
  const evidencePercent = evidence.length > 0 ? FEATURE_STATE_WEIGHTS[state] : 0;
  const contributionPoints = Math.round(weight * evidencePercent) / 100;
  return Object.freeze({
    id,
    title: text(raw?.title, id),
    weight,
    state,
    evidencePercent,
    contributionPoints,
    summary: plainText(raw?.summary),
    evidence: Object.freeze(evidence),
    caveat: plainText(raw?.caveat),
  });
}

function featureWeight(feature) {
  if (feature?.ownerExcluded === true) return 0;
  return Number(feature?.evidencePercent) || 0;
}

// AGENT-CONTRACT: breadth answers "how much of the plan has any integrated
// evidence?" It deliberately ignores maturity above MODELLED and never counts
// ABSENT/unrecorded or candidate-only work. Keep it separate from progress so
// callers cannot add the two percentages together.
function featureBreadth(feature) {
  if (feature?.ownerExcluded === true) return 0;
  if (feature.steps.length === 0) return feature.evidencePercent > 0 ? 100 : 0;
  return feature.steps.reduce(
    (sum, step) => sum + (step.evidencePercent > 0 ? step.weight : 0),
    0,
  );
}

function normalizeFeature(raw, index) {
  const id = text(raw?.id);
  if (!FEATURE_ID_PATTERN.test(id)) throw new TypeError(`features[${index}].id must be a QQ source row id`);
  const state = text(raw?.state, 'UNVERIFIED').toUpperCase();
  if (!(state in FEATURE_STATE_WEIGHTS)) throw new TypeError(`features[${index}].state is unsupported`);
  const stoppingPoint = raw?.stoppingPoint && typeof raw.stoppingPoint === 'object'
    ? raw.stoppingPoint : {};
  const evidence = normalizeEvidence(stoppingPoint.evidence);
  const steps = Array.isArray(raw?.steps)
    ? raw.steps.map((step, stepIndex) => normalizeStep(step, stepIndex, id)) : [];
  if (new Set(steps.map((step) => step.id)).size !== steps.length) {
    throw new TypeError(`${id}.steps must have unique ids`);
  }
  const stepWeight = steps.reduce((sum, step) => sum + step.weight, 0);
  if (steps.length > 0 && Math.abs(stepWeight - 100) > 0.001) {
    throw new TypeError(`${id}.steps weights must total 100`);
  }
  const evidencePercent = steps.length > 0
    ? Math.round(steps.reduce((sum, step) => sum + step.contributionPoints, 0) * 100) / 100
    : (evidence.length > 0 ? FEATURE_STATE_WEIGHTS[state] : 0);
  const feature = {
    id,
    title: text(raw?.title, id),
    workspace: text(raw?.workspace, 'Unspecified'),
    state,
    ownerExcluded: raw?.ownerExcluded === true,
    evidencePercent,
    steps: Object.freeze(steps),
    stoppingPoint: Object.freeze({
      summary: plainText(stoppingPoint.summary),
      evidence: Object.freeze(evidence),
    }),
    caveat: plainText(raw?.caveat),
  };
  return Object.freeze({
    ...feature,
    evidenceBreadthPercent: featureBreadth(feature),
  });
}

// AGENT-GUARD: liveness fails closed. A record is active only when its status
// starts with `working` and its newest declared update is neither stale nor in
// the future beyond clock skew. Anything else must read as inactive.
function workerIsFresh(worker, nowMs, maxAgeMs) {
  if (!worker.recordValid) return false;
  if (!/^working\b/i.test(worker.status)) return false;
  const updatedAt = timestamp(worker.updatedAt);
  if (updatedAt <= 0 || updatedAt > nowMs + WORKER_CLOCK_SKEW_MS) return false;
  return nowMs - updatedAt <= maxAgeMs;
}

export function buildBoard(data, workers, options = {}) {
  const nowMs = Number.isFinite(options.nowMs) ? options.nowMs : Date.now();
  const maxWorkerAgeMs = Number.isFinite(options.maxWorkerAgeMs)
    ? options.maxWorkerAgeMs : WORKER_ACTIVITY_MAX_AGE_MS;
  const sourceRows = Number(data?.program?.sourceRows);
  if (!Number.isInteger(sourceRows) || sourceRows < 1) {
    throw new TypeError('program.sourceRows must be a positive integer');
  }
  const sourceFeatures = Array.isArray(data?.features) ? data.features : [];
  const features = sourceFeatures.map(normalizeFeature);
  if (features.length > sourceRows) throw new TypeError('feature records exceed program.sourceRows');
  if (new Set(features.map((feature) => feature.id)).size !== features.length) {
    throw new TypeError('feature records must have unique ids');
  }
  const ownerExcludedRows = features.filter((feature) => feature.ownerExcluded).length;
  const actionableRows = sourceRows - ownerExcludedRows;
  const recordedActionableRows = features.filter((feature) => !feature.ownerExcluded).length;
  const qualified = features.filter((feature) => featureWeight(feature) === 100).length;
  const evidencePoints = Math.round(features.reduce((sum, feature) => sum + featureWeight(feature), 0) * 100) / 100;
  const evidenceBreadthPoints = Math.round(
    features.reduce((sum, feature) => sum + featureBreadth(feature), 0) * 100,
  ) / 100;
  const currentWorkers = workers.map((worker) => Object.freeze({
    ...worker,
    active: workerIsFresh(worker, nowMs, maxWorkerAgeMs),
  }));
  const orderedWorkers = currentWorkers.sort((left, right) => (
    Number(right.active) - Number(left.active)
    || timestamp(right.updatedAt) - timestamp(left.updatedAt)
    || left.name.localeCompare(right.name)
  ));
  const providers = (Array.isArray(options.providers) ? options.providers : [])
    .map(normalizeProvider);
  if (new Set(providers.map((provider) => provider.id)).size !== providers.length) {
    throw new TypeError('provider records must have unique ids');
  }
  const messages = orderedWorkers.flatMap((worker) => worker.updates.map((update, index) => ({
    worker: worker.name,
    at: update.at || worker.updatedAt,
    message: update.message,
    order: index,
  }))).sort((left, right) => timestamp(right.at) - timestamp(left.at) || left.order - right.order);

  return Object.freeze({
    program: Object.freeze({
      name: text(data?.program?.name, 'Team Board'),
      sourceRows,
      actionableRows,
      ownerExcludedRows,
      recordedRows: features.length,
      recordedActionableRows,
      unverifiedRows: sourceRows - features.length + features.filter((feature) => feature.state === 'UNVERIFIED').length,
      qualified,
      providerCount: providers.length,
      availableProviders: providers.filter((provider) => provider.available).length,
      evidencePoints,
      evidencePercent: actionableRows > 0 ? Math.round(evidencePoints / actionableRows * 100) / 100 : 0,
      evidenceBreadthPoints,
      evidenceBreadthPercent: actionableRows > 0
        ? Math.round(evidenceBreadthPoints / actionableRows * 100) / 100 : 0,
      formula: 'Each roadmap row is 100 breadth points. Named sub-outcomes contribute their explicit weight × evidence maturity: UNVERIFIED/ABSENT 0, MODELLED 25, WIRED 50, EXECUTABLE 75, QUALIFIED 100. Rows without a breakdown retain the same state score. Only recorded evidence counts; candidate branches and worker activity add zero, and task percentages add zero.',
      breadthFormula: 'Integrated footprint counts the weight of named outcomes with recorded evidence at MODELLED, WIRED, EXECUTABLE, or QUALIFIED maturity. It excludes ABSENT/unrecorded and candidate-only work. It is coverage, not completion, and is never added to the maturity score.',
    }),
    features: Object.freeze([...features].sort((left, right) => left.id.localeCompare(right.id))),
    workers: Object.freeze(orderedWorkers),
    providers: Object.freeze(providers),
    messages: Object.freeze(messages),
    workerErrors: Object.freeze(Array.isArray(options.workerErrors) ? [...options.workerErrors] : []),
  });
}

// AGENT-CONTRACT: outcome rows are owned by the canonical repository record.
// A --team-root coordination worktree may predate additions to
// ops/team/features.json, in which case the canonical file still defines the
// program rows while workers and messages come from the requested root.
const canonicalFeaturesPath = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../ops/team/features.json');
const canonicalProvidersPath = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../ops/team/providers.json');

export async function readBoard(teamRoot) {
  const featurePath = path.join(teamRoot, 'features.json');
  const workersPath = path.join(teamRoot, 'workers');
  let data;
  let providerData;
  try {
    data = JSON.parse(await readFile(featurePath, 'utf8'));
  } catch (error) {
    if (error?.code !== 'ENOENT') throw error;
    data = JSON.parse(await readFile(canonicalFeaturesPath, 'utf8'));
  }
  try {
    providerData = JSON.parse(await readFile(path.join(teamRoot, 'providers.json'), 'utf8'));
  } catch (error) {
    if (error?.code !== 'ENOENT') throw error;
    providerData = JSON.parse(await readFile(canonicalProvidersPath, 'utf8'));
  }
  // AGENT-NOTE: unlike the flow upstream, this repo keeps a README inside
  // workers/ describing the record convention; it is documentation, never an
  // employee record.
  // AGENT-CONTRACT: Flow/Sloom employee history is the durable organization.
  // Never use a separately maintained roster as a visibility switch: it can
  // lag a handoff and hide a genuine worker, reviewer, or help offer. The
  // manager enforces the live-process capacity ceiling operationally; this
  // reader shows every valid employee record and lets status + freshness fail
  // liveness closed.
  const fileNames = (await readdir(workersPath))
    .filter((fileName) => fileName.endsWith('.md') && fileName !== 'README.md')
    .sort();
  const workerResults = await Promise.all(fileNames.map(async (fileName) => {
    try {
      return { worker: parseWorkerMarkdown(await readFile(path.join(workersPath, fileName), 'utf8'), fileName) };
    } catch (error) {
      return { error: normalizeWorkerError(fileName, error) };
    }
  }));
  const workers = workerResults.flatMap((result) => result.worker ? [result.worker] : []);
  const workerErrors = workerResults.flatMap((result) => result.error ? [result.error] : []);
  return buildBoard(data, workers, { workerErrors, providers: providerData?.providers });
}
