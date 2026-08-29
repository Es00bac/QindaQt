# Iris Hale consolidated audit: Display D1 modules vs the seven contracts

- **Timestamp:** 2026-08-27T17:41:43-06:00
- **From:** Iris Hale, Display D1 adversarial audit assistant
- **To:** Display D1 lead/keeper
- **State:** terminal audit for this engagement; read-only static inspection
  only. No candidate acceptance is claimed or implied.
- **Evidence identity:** worktree
  `/home/cabewse/work_SPaC3/container-wm-workers/display-d1`, branch
  `worker/display-d1`, HEAD `94e84077e33a279dcebee24511e7dbdf1b87e3e1`
  (equals the assignment's exact public base), plus the lead's uncommitted
  trees `src/services/display_{protocol,identity,topology,transaction}/` as of
  2026-08-27T17:41-06:00. The lead edited concurrently during the audit; every
  line citation below was re-verified against the tree at inspection time.
  All findings are static source facts; no compiler, test, or runtime evidence
  exists or is claimed.

## Contract coverage summary

- **C1 (typed bounds, total fail-closed decode, no partial replacement):**
  implemented and coherent. Bounds are explicit
  (`display_limits.h:10-36`, `identity_limits.h`, `transaction_types.h:14-16`);
  byte/journal codecs are total, bounded before allocation, magic+version
  checked, trailing-byte checked, and replace destinations only after full
  validation (`display_codec.cpp:41-87`, `display_codec_snapshot.cpp:156-218`,
  `transaction_journal.cpp:141-199`). Residual protocol-DBus hardening items
  in the interim message.
- **C2 (identity precedence, ambiguity, no leakage):** precedence
  identifier→raw-EDID→MST→connector fallback is exactly implemented
  (`identity_resolver.cpp:175-188`); duplicates resolve to distinct
  connector-fallback IDs with `ambiguous=true` (189-192); collision suffixing
  is deterministic (199-213) and defense-in-depth only; MST composite is
  collision-free under NUL-free paths (44-59); published IDs are prefix+hex
  digest only — no serial/raw-EDID material. Malformed EDID material is never
  hashed (140-158).
- **C3 (pure registry, rename/hotplug reconciliation, UUID non-authority,
  typed errors):** all registry functions are pure value-in/value-out with
  typed `RegistryError`s and strict key-set/schema checks, v1→v2 migration
  flag, canonical sorted output (`identity_registry.cpp` throughout).
  Compositor UUID non-authority is enforced by construction: no registry or
  resolved type stores it. Alias rules (regex, ambiguity, duplicates) are
  deterministic (43-49, 298-327).
- **C4 (topology validation, KWin rounding, fingerprints, diffs, no-op):**
  implemented, including lineage/set checks, primary/priority rules, mirror
  source/self/cycle rules, overlap and gap-warning logic, `(0,0)`
  normalization with overflow-bounded math, canonical sorted outputs, and
  `noOp` from diff emptiness (`topology_validation.cpp`). The rounding formula
  `floor(x+0.5)` (`topology_geometry.cpp:39-40`) reproduces the accepted
  table row 2560×1440@150% → 1707×960. Caveat: I could verify only against the
  accepted table, not pinned KWin source — the reference page must pin the
  full table and the KWin 6.6.5 derivation. Finding **F2** below qualifies
  no-op truth.
- **C5 (transaction machine):** injected clock/port only
  (`transaction_ports.h`), one transaction (`kMaxTransactions=1`, single
  `m_staged`), base-revision fencing (`transaction_machine.cpp:142-145`),
  full stage/apply/observe/confirm/cancel/deadline/revert surface, typed
  `CommandError`s, journal values, three bounded revert attempts
  (`transaction_machine_revert.cpp:123-152`), `Stuck` truth, and no uncertain
  forward replay (uncertainty resolves only via preimage/staged observation or
  revert; no forward re-issue). **Finding F1 is a liveness defect.**
- **C6 (hotplug settle, surviving-properties-only revert, external intent,
  lock/suspend, crash recovery):** deterministic `topologySettled` input
  (`transaction_machine_events.cpp:204-225`);
  `SurvivingOutputProperties` carries only modeId/scale/transform
  (`transaction_types.h:90-98`) and the revert request sends properties-only
  scope — no enable/position/priority/replication replay
  (`transaction_machine_revert.cpp:90-105`); external-newer intent aborts
  without fighting (`transaction_machine_events.cpp:143-167,131-139`); lock
  and suspend revert (`227-260`); `recover()` drives the same
  SettlingTopology/Reverting path (`transaction_machine_revert.cpp:167-207`).
- **C7 (class-A confirmation, closed class-B):** `ChangeClass` is a closed
  14-value enum and `confirmationRequirement` is a total switch that fails
  safe to `Required` for unknown values with an AGENT-GUARD
  (`display_validation.cpp:295-319`). Ownership/lifetime/threading/error and
  port pre/postconditions are documented in headers (`identity_resolver.h:18-21`,
  `topology.h:16-18`, `transaction_machine.h:14-16`,
  `transaction_ports.h:22-27`). Per-port pre/postcondition prose for D2
  authors still needs the reference page.

## Findings (requested lead action at the end)

**F1 — tick() has no `RevertingApply` branch: hung revert apply never
retries and never reaches Stuck. Severity: High.**
`transaction_machine_events.cpp:262-298`: tick handles Applying,
ResolvingUncertain, Observing, AwaitingConfirmation, RevertingObserve, and
RevertBackoff, but not `RevertingApply`, although `issueRevertApply` sets an
apply-timeout deadline in that state
(`transaction_machine_revert.cpp:132-136`). If the port never delivers
`applyCompleted` for a revert token, the deadline expires and tick returns
`accepted(false)` forever: the "three bounded revert attempts" only bound
failing callbacks, not silent ones, and `Stuck` is unreachable. Suggested
repair direction: on deadline in `RevertingApply`, call
`scheduleRevertRetry()` (mirroring the RevertingObserve branch), plus a
fake-clock test row for a never-calling-back revert port.

**F2 — disabled-output normalization breaks baseline no-op truth.
Severity: Medium (conditional on adapter data, but the model permits it).**
`validateOutput` rejects disabled outputs only for primary/priority
(`display_validation.cpp:171`); a disabled snapshot output may legally carry
non-zero `position` and non-empty `replicationSourceStableId` (replication
source existence is checked at `display_validation.cpp:253-258`).
`validateAndNormalize` forces disabled candidate outputs to position (0,0)
and cleared replication (`topology_validation.cpp:169-174`), while
`candidateFromSnapshot` copies the snapshot verbatim
(`topology_fingerprint.cpp:43-67`) and `diff` compares Position and
ReplicationSource for disabled non-replicated outputs
(`topology_fingerprint.cpp:110-123`). Consequences for such snapshots:
(a) staging the derived baseline is not `noOp` (spurious
Position/ReplicationSource diffs); (b) the machine's `validSnapshot`
convention — `liveFingerprint == canonicalFingerprint(candidateFromSnapshot())`
(`transaction_machine.cpp:65-72`) — and the normalized-candidate fingerprint
(`topology_validation.cpp:312`) disagree on those outputs, so a fingerprint
that passes `initialize` is not the fingerprint stage/persist would compute
for the equivalent baseline. Suggested repair directions (lead's choice, then
document + test): either make `validateSnapshot` require disabled outputs to
have position (0,0) and empty replication source (canonical-by-construction
snapshot), or normalize `candidateFromSnapshot` identically to
`validateAndNormalize` and define fingerprint over the normalized form.

**F3 — snapshot fingerprint convention is an implicit cross-module contract.
Severity: Medium (documentation/contract risk).**
`Machine::validSnapshot` requires `liveFingerprint` to equal
`canonicalFingerprint(candidateFromSnapshot(snapshot))`
(`transaction_machine.cpp:70-71`). This binds every future D2 adapter and the
D1 reference page, but no header or wiki states it. With F2 unresolved it is
also self-inconsistent (see F2b). Requested: state the convention as an
AGENT-CONTRACT at the Snapshot type or in the reference page, and pin it with
round-trip tests.

**F4 — in-flight artifacts absent: tests, wiki pages, ADR-0015/0016, build
registration. Severity: Medium process fact, expected for a working tree.**
`tests/services/display_{identity,protocol,topology,transaction}/` exist but
contain only two empty `support/` directories; `docs/wiki/architecture/
display-service.md`, `docs/wiki/reference/display1-v1.md`, and
`docs/wiki/adr/0015*`/`0016*` do not exist; no top-level
`CMakeLists.txt`/`src/**/CMakeLists.txt` modification adds the four modules
(`git status` shows only the four untracked source dirs), so nothing can
configure/build them yet. Consistent with the claim's checkpoint promise;
recorded so the audit boundary is explicit. The acceptance-evidence rows
(fuzz-style hostile rows, fake-clock state edges, KWin rounding table,
ownership/dependency gates) remain entirely unproven — no test evidence
exists, and I claim none.

**F5 — `kCoordinateBound = 1'000'000` provenance unpinned. Severity: Low.**
`display_limits.h:32`. Contract 4 requires "KWin's coordinate bound". I could
not verify the constant against the pinned KWin 6.6.5 source from this tree;
the reference page/ADR must cite the exact KWin derivation, and the rounding
table + bound need nested/measured confirmation per the M0 row before D2.

**F6 — policy choices needing explicit documentation or tests. Severity:
Low/question.**
1. Priority contiguity 1..N is stronger than KWin uniqueness
   (`topology_validation.cpp:147-163`) — document as a QindaQt canonical
   policy or align to KWin.
2. Chained replication (A→B→C) is permitted unless cyclic
   (`topology_validation.cpp:204-240`); confirm the pinned protocol/KWin
   accepts chained `replicationSource`, else reject non-root sources.
3. `storeJournal` results are deliberately ignored on
   topology/`beginRevert`/`enterStuck` update paths
   (`transaction_machine_events.cpp:200`,
   `transaction_machine_revert.cpp:119,131,164`) — defensible (an earlier
   journal exists or Stuck must record), but state the rationale once so a
   future editor does not "fix" it.
4. `TopologyChanged` from `Ready` accepts any newer/valid snapshot but
   `observedSnapshot` from `Ready` rejects epoch changes
   (`transaction_machine_events.cpp:62-65` vs `177-181`) — the adapter-side
   selection rule between the two entry points should be documented.
5. `safeText` rejects only `Other_Control`; format-control characters (e.g.
   U+202E) pass into labels/diagnostics (`display_validation.cpp:26-34`,
   `identity_registry.cpp:35-39`). Consider `Other_Format` rejection for
   user-visible text.
6. `kServiceName`/`kObjectPath`/`kInterfaceName` constants exist in D1
   (`display_limits.h:38-40`). Defining the future service identity is not
   creating a service, and no registration/connection code exists; keep it
   that way until D2 and say so in the reference page so the forbidden-artifact
   audit stays clean.

**F7 — source shape and dependencies: clean.**
Largest file is 329 lines (`identity_registry.cpp`), well under the 500-line
review threshold. Forbidden-dependency sweep found no KWin/Wayland/QML/
Settings/logind/kscreen/QObject/QTimer/QFile/real-clock usage in any of the
four modules (only a descriptive KWin comment at
`topology_validation.cpp:187`); `display_identity`/`display_topology`/
`display_transaction` link `Qt6::Core` (topology/transaction additionally
their sibling targets), and only `display_protocol` links `Qt6::DBus`
(`*/CMakeLists.txt`), exactly as the manager outcome allows. No
service/name/XML/client/UI/journal-file or compositor-mutation artifact
exists.

## Requested lead action

1. Triage F1 (High) for repair in this worktree; F2/F3 need a design decision
   (canonical-snapshot validation vs mirrored normalization) before tests pin
   them; F5/F6 need documentation or a pinned-source citation.
2. Treat F4 as the known in-flight state; the audit is scoped to source only
   and all acceptance rows remain open until tests/docs/ADRs exist.
3. My interim re-evaluation message of the same timestamp supersedes the two
   interim findings' wording; both remain active inputs for your review.
4. Per my assignment I claim no candidate acceptance; review routing and any
   handoff remain yours.
