# Iris Hale exact-commit adversarial review: Display D1 `0a8d0e0e` — PASS (no blocking finding; all claimed repairs verified present in the commit object)

- **Timestamp:** 2026-08-28T05:45:40Z
- **From:** Iris Hale, Display D1 adversarial audit assistant (GLM,
  `zai-coding-plan/glm-5.3-flash`, high)
- **To:** Kellan Ward, Display D1 transaction implementer and lead; cc Display
  D1 manager; Elara Finch remains the named exact-candidate rereviewer
- **Exact candidate:** commit `0a8d0e0eac9e0d7c5932fb54b875667b5d7f1639`,
  tree `63617b3a07620b237a74cf2416191d61cd866d3e`, single parent
  `0e38fa726af69e34be3cacdd6b71d40350ac8092`, series base
  `94e84077e33a279dcebee24511e7dbdf1b87e3e1` — all four hashes resolved
  directly from the Git object store in worktree
  `/home/cabewse/work_SPaC3/container-wm-workers/display-d1`, whose HEAD
  equals the candidate and whose only untracked paths (`.omc/`,
  `ops/team/workers/kai-mercer.md`) are absent from the tree. Subject,
  15-path scope, and `245 insertions, 26 deletions` match handoff
  `1787895385` exactly.
- **Verdict:** **PASS** as my adversarial record — no P0–P2 finding; the
  commit closes Elara's exact-candidate P1, both P2s, and all six P3 items,
  plus Mina Shah's P0, exactly as claimed. I verified the commit object, not
  the handoff prose; Kellan's compiler/CTest/sanitizer/package/doc-gate counts
  remain his lane's evidence and were not adopted by me. This is not candidate
  acceptance: integration stays with the manager after Elara's rereview.
- **Evidence boundary:** static reading only. I read every changed hunk from
  the commit via `git show`/`git diff`, plus the committed journal, recovery,
  validation, and fingerprint sources, both test-support headers, the public
  header inventory, and the reason/limit headers. No edit, commit, configure,
  build, test execution, display/session, or host-state interaction by me.

## 1. Identity and scope (verified, not trusted)

- `rev-parse` confirms commit/tree/parent as handed off; `git log` confirms
  the two-commit series `94e84077 → 0e38fa72 → 0a8d0e0e` with no squashed
  extras; `--stat` confirms exactly 15 paths, `+245/−26`.
- All 15 paths are D1-owned: `display-service.md`, ADR-0015,
  `display_protocol` (`display_limits.h`, `display_validation.cpp`),
  `display_topology` (`topology_fingerprint.cpp`), `display_transaction`
  (3 public headers, `transaction_machine_events.cpp`,
  `transaction_machine_revert.cpp`), and the five focused display test files.
  No shared registry (`src/CMakeLists.txt`, `tests/CMakeLists.txt`,
  `mkdocs.yml`) is touched — correct, because the commit adds no files.
- Because the worktree is clean against HEAD, my prior verdicts
  (`1787889831`, `1787893872`) cited this same repair content as an
  uncommitted tree; every line citation I re-checked in the committed blobs
  matches those citations. Nothing changed between what I audited and what
  was committed.

## 2. Repair verification against the Elara/Mina matrix

- **Elara P1 (mirror projection, blocking) — closed.** The committed
  `candidateFromSnapshot` (`topology_fingerprint.cpp:75-124`) now computes
  the origin minimum over enabled non-replica outputs (`:76-90`), resolves
  every replica transitively to its ultimate root in a first pass
  (`:92-107`, cycle-bounded `steps++` guard), and only then translates all
  enabled outputs once (`:109-116`, AGENT-GUARD at `:109-111`). Roots are
  never mutated before all reads, the minimum is a pure function of the
  output set, and the closing sort by stable ID is canonical — the result
  is order-independent for root-first and replica-first lists, including
  cycle and orphan-chain inputs (deterministic set functions in all cases).
  Structural agreement with `validateAndNormalize` is now exact, not
  incidental: `canonicalizeMirrors`
  (`topology_validation.cpp:222-249`) resolves chains to the ultimate root
  from untranslated values and `normalizePositions` (`:251-266`) performs
  the single translation — the same two-pass shape.
- **Regression rows are genuine, not vacuous.** Both support helpers
  hard-code `.enabled = true` (`topology_test_data.h:33`,
  `transaction_test_support.h:98`), so
  `translatedMirrorProjectionIsOrderIndependent`
  (`tst_topology_candidate.cpp:103-139`) and
  `translatedMirrorRollbackConvergesAfterOriginRestore`
  (`tst_transaction_adversarial.cpp:133-173`) exercise an *enabled*
  replica; on `0e38fa72` the first fails its candidate-equality assert
  (replica `(-100,-50)` vs `(0,0)`) and the second fails the
  `FullPreimage` rollback assert. No-op truth, fingerprint truth, and
  translated-mirror rollback are pinned in both output orders as required.
- **Elara P2.1 (settle routing) and P2.2 (Staged routing) — closed at the
  contract level Elara specified.** `transaction_ports.h:43-47` states both
  adapter rules; `display-service.md` carries them into the state table
  (`Staged` row), the side-effect port preconditions, and the settle
  section ("route that intent before entering the settle window").
- **Elara P3.1 — closed.** `Output` and `Snapshot` D-Bus signatures are now
  asserted (`tst_display_protocol_codec.cpp:117-118,124-125`) and equal the
  marshaller strings Elara hand-verified.
- **Elara P3.2 — closed.** Both `externalIntentObserved` abandon paths now
  persist `reason = ExternalChange` before `clearJournal()`
  (`transaction_machine_events.cpp:227-236,244-254`, AGENT-GUARD
  `:245-247`); on clear failure `enterStuck(true, ExternalChange)`
  preserves the crash-recovery instruction
  (`transaction_machine_revert.cpp:176,193-197`, AGENT-CONTRACT
  `:194-195`). The new restart rows
  (`tst_transaction_recovery.cpp:234-257`) pin the failed-clear Stuck
  journal reason and the changed-set recovery that settles and abandons
  without issuing any apply.
- **Elara P3.3 — closed.** All three Ready entry points now share
  `followsCurrentLineage` (`transaction_machine_events.cpp:11-19`; gates at
  `:102`, `:216-218`, `:266-268`), and
  `readyInputsEnforceCurrentLineage` (`tst_transaction_state.cpp:54-91`)
  pins older/other-epoch rejection plus newer acceptance for both
  `externalIntentObserved` and `topologyChanged`, asserting exact
  view/snapshot preservation on rejection.
- **Elara P3.4 and Mina P0 — closed.** `display_limits.h:18` names
  `kMaximumRevertAttempts = 3`; `display_validation.cpp:216` consumes it;
  `transaction_types.h:5,16` includes `display_limits.h` directly and
  aliases the constant, keeping the public name and type (`quint32`) while
  closing the self-containment gap without changing dependency direction
  (transaction → protocol is the existing PUBLIC link).
- **Elara P3.5, P3.6 — closed.** ADR-0015 gains the logind
  delay-inhibitor/rollback-after-resume sentence;
  `display-service.md` documents the deliberate pending-reason retention
  on uncertain completion after rollback was requested.

## 3. Adversarial counterexample hunt (commit-scoped) — no blocker

- **State transitions.** The new Ready-path rejections strictly fence:
  stale or cross-epoch deliveries are refused before any mutation and the
  rejection preserves `view()`/`currentSnapshot()` exactly (pinned); the
  non-Ready observation window is unchanged (the documented adapter
  delivery window from my prior Low note 1 remains, still no action).
  `retryStuck` keys on `m_cleanupOnlyStuck`, not the journal reason, so the
  durable-reason override cannot reroute retry behavior.
- **Topology/identity.** Order-independence argument above; survivor
  identity, output-set identity, and duplicate-ID rejection are untouched
  by this commit. `journal reason = ExternalChange` is within the
  gap-free validity range (`ExternalChange = 6 ≤ TransportUncertain = 14`,
  `display_types.h:38-54`), so `isValidJournal`'s range-only check accepts
  every new phase/reason combination the crash windows can produce — no
  bricked-journal path exists.
- **Failure recovery.** Traced every new crash window: (a) die between
  `storeJournal(ExternalChange)` and `clearJournal` → restart lands in
  `recover()`'s `ExternalChange` branch (`transaction_machine_revert.cpp`
  :227-243): changed set vs preimage settles and abandons, same set clears
  and readies — never a pre-image replay over external truth;
  (b) failed clear without crash → `Stuck` journal carries
  `ExternalChange`, pinned by the restart rows; (c) failed store before
  clear is no worse than the prior code and strictly better when either
  call succeeds. No path added by this commit issues an apply.
- **Serialization/boundaries.** Only `display_validation.cpp` and the
  public headers changed on the protocol side: the single literal-to-named-
  limit swap is behavior-identical, and `transaction_machine_events.cpp`
  grows to 462 non-blank lines — still under the 500-line review
  threshold. Installed public header inventory remains exactly 15
  (4 identity, 5 protocol, 2 topology, 4 transaction), matching the staged
  install claim; `transaction_machine.h`'s new default argument is
  source- and ABI-additive within the module.

## 4. Findings

- **P0–P2:** none.
- **P3 (Low, no action requested):**
  1. `recover()`'s own clear-failure branches still call
     `enterStuck(true)` without a durable reason
     (`transaction_machine_revert.cpp:236,261`), so a Stuck journal
     produced there reports `JournalFailure` even when the dying
     instruction was `ExternalChange`. Traced safe — every such branch has
     the live set equal to the preimage set or matching an endpoint, so any
     restart resolves to clear/settle without fighting external truth; this
     is reason-fidelity nuance in pre-existing code, not a defect
     introduced here. A future one-line symmetry pass could pass the
     durable reason through.
  2. The persist-before-clear step writes the journal with its pre-abort
     phase (e.g. `Committed`) rather than `Stuck`
     (`transaction_machine_events.cpp:229-230,246-247`). This is exactly
     what the AGENT-GUARD intends — the reason steers recovery — and it is
     safe only because journal validity is range-based, not
     phase/reason-coupled. Worth remembering if anyone ever tightens
     `isValidJournal` to phase/reason coupling: that change would need to
     revisit this window first.
- **Carried Low notes (unchanged, no action):** active-state observation
  lineage window and reason relabel across the settle barrier, both from
  verdict `1787893872`.

## 5. Evidence limits and requested action

Static reading only; I executed no gate, and the pass/fail counts in
handoff `1787895385` remain Kellan's compiler-lane evidence — nothing in
the commit contradicts them, and every row I cite is present in the
committed blobs. My PASS is the adversarial record, not acceptance: Elara
Finch should proceed with the requested rereview of exactly
`0a8d0e0eac9e0d7c5932fb54b875667b5d7f1639` (reproducing the `[A,B]`/`[B,A]`
translated traces first, per handoff `1787895385`), and only the manager
integrates the two-commit series onto public main. Kellan: no action
required; the two P3 notes above need no repair.
