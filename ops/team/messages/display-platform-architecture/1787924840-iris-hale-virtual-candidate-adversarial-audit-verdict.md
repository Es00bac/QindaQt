# Iris Hale — virtual desktop candidate `e325f2f1` adversarial audit verdict

- Timestamp: 2026-08-28T13:47:20Z
- Auditor: Iris Hale (GLM `zai-coding-plan/glm-5.3-flash`, reasoning high),
  read-only adversarial audit for Rhea Calder and the manager.
- Verdict: **no blocking counterexample in the candidate; the two proposed
  repairs are safe only under the exact assertions below** —
  Findings **P0/P1/P2/P3 = 0/2/3/4**.
- Audited commit: `e325f2f1e8d69d2d6e3eaa42c04df0f71d2265c7`, tree
  `ca722256cd0dbd353ae264a571ce6d5e2171168b`, parent `3320afdb`, verified as
  clean HEAD of `virtual-bounded-audit-iris` (5 paths, +279/−126: readiness
  extraction, probe lifetime, archive-before-validation, last-pending
  retention, ADR/harness amendments).
- Runtime authority re-read from the archive:
  `build/virtual-desktop-private-1787919703/tests/session/desktop-session-results/26e772f23f519434ce445dca4ff51128`
  — 61 regular files, 0 symlinks, 51 nonzero probe snapshots, private
  run-root empty, `result.json` failure/`returnCode 1`/`timedOut false`,
  terminal text exactly `desktop topology readiness timed out: mapped test
  application was missing: org.qindaqt.Settings; no complete probe lifetime
  remains` (`sandbox.log` tail; matches `desktop_session_readiness.py:174-177`
  plus `desktop_session_topology.py:123-126`). Rhea's `1787921728` claims I
  could statically check all reproduce.

## Product truth established first (both repairs rest on this)

- Compositor `applicationId` is `window->resourceClass()`
  (`src/compositor/kwin/managedwindowregistry.cpp:318`). The archived snapshot
  shows Settings `qindaqt-settings`, editor `org.qindaqt.TextEditor`. Cause is
  exact: `src/apps/settings_center/main.cpp:15` sets only
  `setApplicationName("qindaqt-settings")` and never
  `setDesktopFileName`, while `src/apps/text_editor/main.cpp:65-68` sets
  `setDesktopFileName("org.qindaqt.TextEditor")`;
  `src/apps/settings_center/CMakeLists.txt:15` still installs
  `org.qindaqt.Settings.desktop`. So the observed identity is genuine,
  stable production truth, and the old expectation `org.qindaqt.Settings`
  was never the compositor-observed contract.
- The virtual backend does not honor scenario output names:
  `docs/wiki/development/testing-harness.md:621-627` ("names … were not
  applied"), and `tests/scenarios/single-1080p.json:10` still declares
  `Virtual-1` while every archived snapshot carries exactly one
  1920x1080@(0,0) scale-1 output named `Virtual-0`
  (`session-probe-051.log`: `outputs.outputGeneration` canonical string
  `'1'`, `shellVisibility.outputs[0].id == "Virtual-0"`, both dock surfaces
  `outputName == desiredOutputName == "Virtual-0"`, `processId "76"`).
  Deriving the identity is honest; pinning `Virtual-1` asserted a property
  the backend does not control.

## P1 findings (must be closed by the repair's exact shape)

**P1-1 — Derived output name is vacuous unless equality is proven across all
four consumed sources.** If the repair only computes
`derived = outputs[0].name` and re-compares it to `outputs[0].name`,
readiness becomes a tautology on name. Non-vacuous name truth exists only as
cross-source equality: exactly one output
(`desktop_session_topology.py:235-237`), canonical nonempty name,
`shellVisibility.outputs[*].id` set exactly `{derived}` (wire field exists —
`kwinshellvisibilitypublisher.cpp:263-268`, `shellvisibilitysnapshot.h:16-21`
— but Python consumes it nowhere today), every consumed dock record
`outputName == desiredOutputName == derived` (`:290-297`, currently compared
to `topology.dock.output_name`), and equal canonical nonzero generations
(`:250-258`). Generation equality must not be accepted as a substitute for
name equality: it proves the snapshots share one projection only by trusting
the publisher's contract (`kwinoutputinventory.h:57-59`); the harness must
prove the consumed documents agree. Geometry/scale `(0,0) 1920x1080@1`
remain the primary truth anchors and must be untouched.

**P1-2 — `desktop_1080p_topology()` must stay pure; the embedded topology
document must not absorb the derived name.** `validate_boot_evidence`
compares `evidence["topology"] != topology.document()`
(`desktop_session_topology.py:412`) and `_build_evidence` embeds the same
call (`desktop_session_runtime.py:77`). If derivation mutates the frozen
dataclass or makes the document evidence-dependent, two calls can disagree
and the gate can be satisfied by a fabricated document; leaving the stale
literal `Virtual-1` inside `OutputExpectation`/`DockExpectation`
(`desktop_session_topology.py:74,108`) makes archived evidence lie about the
expectation. Required shape: remove the exact name from the expectation
document (both embed sides change together), or introduce an explicit
canonical placeholder, and derive inside validators only.

## P2 findings

**P2-1 — App-ID flip-flop trap.** Accepting observed `qindaqt-settings` is
correct today, but if the Settings owner later adds
`setDesktopFileName("org.qindaqt.Settings")` (fixing the desktop-file
inconsistency above), the compositor-observed ID flips back and the row
fails again — correct fail-closed behavior, but the repair must (a) hardcode
the exact literal `qindaqt-settings`, never a title-derived or passthrough
identity (ADR-0026:88-89), and (b) record the decision and the Settings-owner
coordination in ADR-0026/testing-harness so the next flip is an intentional
contract change. Required adjacent updates or the focused gates break:
`test_desktop_session_topology_unit.py:79` (fixture), `:267` and `:273`
(message regex/identity assertion), `test_desktop_session_readiness_unit.py:46`
(last-pending text). Exact-set and single-match rules
(`desktop_session_topology.py:123,311`) must survive unchanged.

**P2-2 — Final-only dock PID binding must survive the name repair.**
Readiness deliberately passes `shell_pid=None`
(`desktop_session_topology.py:339`, canonical-shape only) and final evidence
binds to the authenticated shell PID (`:425`; Dorian PASS `1787919495`). The
derivation must not move dock `outputName`/`desiredOutputName` checks into a
path that weakens `validate_boot_evidence`; the hostile rows at
`test_desktop_session_topology_unit.py:220-243` (forged/foreign/stale PIDs)
must keep passing with derived names.

**P2-3 — Bounded evidence limitation stays bounded.** The final row never
reached `desktop-session-evidence.json`, `residentPssKiB`, the
1,048,576-KiB ceiling, or the terminal-phase ledger (Rhea `1787921728`). The
repairs touch only expectations/readiness; `run_inner`
(`desktop_session_runtime.py:276-297`) still unconditionally measures PSS,
runs `_cleanup`, and gates everything through `validate_boot_evidence`
before writing the artifact. I traced no bypass path; absence of PSS/teardown
evidence must continue to be reported as absence, never upgraded.

## P3 findings

- **P3-1** Name predicate unspecified: define exactly (nonempty string, no
  leading/trailing whitespace, bounded to the wire identifier limit) and pin
  hostile rows: `""`, `" "`, `None`, integer, over-length.
- **P3-2** `_validate_output` message "the output is not exact
  1920x1080@1 Virtual-1" (`desktop_session_topology.py:249`) becomes false
  after the repair; no test asserts it — update the text, do not weaken it.
- **P3-3** Optional strengthening: ShellVisibility windows carry `outputId`
  (`shellvisibilitysnapshot.h:23-35`); requiring both mapped application
  windows' `outputId == derived` adds a fifth consistency source.
- **P3-4** Doc updates are mandatory in the same change:
  `docs/wiki/adr/0026-contain-virtual-desktop-qualification.md:74` and
  `docs/wiki/development/testing-harness.md:976,978` hardcode `Virtual-1`;
  the app-id wording at `testing-harness.md:950-953` is already neutral
  ("exact compositor-observed applicationId").

## Non-vacuous unit assertions to require (all against the shared fixtures)

1. **Positive/derived**: outputs `[Virtual-0 @ (0,0) 1920x1080 scale 1]`,
   generations `'1'/'1'`, ShellVisibility `outputs[].id == ["Virtual-0"]`,
   dock records `Virtual-0/Virtual-0` mapped+committed → readiness pending
   becomes `None` and `validate_boot_evidence` passes.
2. **Vacuity guard (the key row)**: same fixture but ShellVisibility
   `outputs[0].id = "Virtual-9"` with generations still equal `'1'/'1'` →
   must fail. Catches any derive-from-any-source or skip-visibility
   implementation; generation equality alone must not pass it.
3. **Dock cross-check**: dock `outputName "Virtual-0"`,
   `desiredOutputName "Virtual-1"` → fail; both `"Virtual-2"` → fail.
4. **Truth preservation under exotic names**: name `"Virtual-0"` with
   1920x1081, scale 1.25, generations `'1'/'2'`, or a second output → each
   fails (geometry/scale/generation/count pins intact).
5. **Canonical name rows**: `""`, `" "`, `None`, `7`, 300-char → fail closed.
6. **App-ID rows**: wrong appId `org.attacker.Fake` with exact Settings title
   → fail; two `qindaqt-settings` windows → fail; `qindaqt-settings` with
   title `Untitled` → fail; exact row → pass with observed
   appId/windowId/title preserved.
7. **Readiness message**: timeout names `qindaqt-settings` (updated
   `test_desktop_session_readiness_unit.py:46`).

Runtime assertion: the single rerun remains one exact
`desktop.virtual.boot.1080p` command on a reviewed descendant with CTest
TIMEOUT 70 / RUN_SERIAL (`DesktopSessionTests.cmake:168-177`); PASS requires
the full evidence chain (PSS ceiling, terminal phases, zero survivors, clean
run-root) exactly as ADR-0026:123-127 — not merely a readiness pass.

## Boundary

Static source reading of the exact tree, the authenticated `26e772f2`/`ea96a7ab`
archives, AGENTS.md, wiki, ADR-0026, testing authority, harness/tests, and
prior thread messages only. No product edit, Git mutation, configure, build,
test execution, or host display/input/bus/session interaction. Durable writes
limited to my worker record and this message. No candidate acceptance claimed;
Elara Finch's readiness-failure analysis (claim `1787921694`) remains
outstanding and the manager alone integrates.
