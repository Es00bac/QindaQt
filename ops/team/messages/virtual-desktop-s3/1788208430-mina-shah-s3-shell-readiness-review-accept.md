# Mina Shah — S3 shell-readiness candidate review: ACCEPT

- Timestamp: 2026-08-31T14:34:00-06:00
- Reviewer: Mina Shah (Anthropic Claude Sonnet 5, high reasoning), independent
  different-worker reviewer
- Candidate reviewed: `4d4b3dc395d80135a8f66f5ffcb2395cf1874c60`
- Tree: `86dbb17efba5c619c5aeb33027264b46a09fb7b9`
- Parent: `39c83a23143f36c3bfec8686b7cbc0121ea8a418`
- Verdict: **ACCEPT for integration**
- Terminal count: **0 P0, 0 P1, 0 P2, 2 P3 (nonblocking)**

## Scope of this review

Read-only inspection in this detached worktree of the exact candidate commit,
its full diff against parent, `AGENTS.md`, `docs/wiki/index.md`, the S1/S2/S3
sections of `docs/wiki/development/testing-harness.md`, and the implementer
handoff at
`virtual-desktop-s3-selene/ops/team/messages/1788207679-dorothy-vaughan-s3-shell-readiness-handoff.md`.
Independently inspected the preserved archive results under
`/tmp/qindaqt-s3-selene-build/tests/session/desktop-session-results/` for the
six named IDs. No configure, compile, CTest, D-Bus, compositor, session, or
input process was started; this is static/archive evidence only.

**Commands I ran myself** (permitted static/read-only validators):
`git diff --check` on the full parent..candidate range (exit 0); `python3 -m
py_compile` on every changed Python file (all OK); `PYTHONDONTWRITEBYTECODE=1
python3 -m unittest discover -s tests/session -p
'test_desktop_session_*_unit.py'` (111/111 passed, matching the handoff);
`python3 -m tools.source_shape.cli --root . --largest 12` (exit 0, 1,757
files, only the two pre-existing unrelated warnings at
`tests/compositor/CMakeLists.txt` and
`tests/services/display_color_model/tst_color_model.cpp`; new files land at
exactly 494 and 499 nonblank lines as claimed); `python3
tools/docs_validation.py` (exit 0, 116/116); `sha256sum` on the dual-row PNG
artifact, matched byte-for-byte against its archived
`matrixCaptures[0].sha256`.

**Commands I did not run**: any CMake configure/build, any `ctest` invocation,
any D-Bus/compositor/session/input startup, any rerun of the S3 matrix. All
runtime conclusions below are archive inspection of the preserved run
directories, not fresh execution.

## Requirement-by-requirement findings

**1. Authentication.** SHA/tree/parent match exactly. `git diff --stat`
confirms 20 changed paths, all under `tests/session/` and
`docs/wiki/development/testing-harness.md`; zero production `src/` files
touched. Working tree is clean (no generated residue).

**2. Fail-closed ShellDevelopment semantics.**
`desktopnotificationshellreadiness.{h,cpp}` (new, 517/494 nonblank lines)
correctly implements every invariant named in the wiki:
- Only `org.freedesktop.DBus.Error.NameHasNoOwner` maps to the transient
  `service-missing` Pending; every other owner-query failure is Invalid.
- Owner stability requires a `:`-prefixed unique name, `ownerAfterSnapshot ==
  owner`, and `serviceProcessId > 1`; any drift is `unstable-owner` (Invalid).
- `serviceProcessId` is joined against `expectation.dockProcessId` (derived
  from every `scope=dock` surface's PID); mismatch is Invalid.
- Cold envelope shape is checked exactly (`size()==2`, `failure.size()==2`,
  `status=="unavailable"`, `code=="service-not-ready"`, nonempty message).
- The Pending whitelist is narrow and exact
  (`service-missing`, `snapshot-object-pending`, `center-window-missing`,
  `center-output-pending`, `privacy-denied` only pre-input,
  `center-open-pending`, plus the pre-sample gate codes
  `public-topology-pending`/`output-pending`/`selected-output-pending`/
  `dock-owner-pending`); everything else is Invalid.
- Ambiguous dock ownership (`dock-owner-ambiguous`, multiple PIDs across dock
  surfaces) is a hard Invalid, checked and returned **before** the unsettled
  check, so it can never be masked as merely Pending. Unsettled
  mapped/committed/output-convergence/geometry is Pending. This exact ordering
  is covered by `tst_desktopnotificationshellreadiness.cpp`'s "conflicting"
  processId row (Invalid) versus its "unmapped"/"unconverged" rows (Pending).

**3. Sole-input causality.** `desktopnotificationbinding.{h,cpp}` adds exact
`qindaqt-shell` component resolution via `getComponent`/`isActive` and
requires `componentActive` before `desktopNotificationBindingReady()` accepts
a binding — registry metadata alone is insufficient, matching the wiki.
`DesktopNotificationActivationObserver` subscribes to the component's
`globalShortcutPressed`/`globalShortcutReleased` signals and
`desktopNotificationActivationComplete()` requires exactly two events, ordered
Pressed-then-Released, both naming the exact component and action. In
`desktopsessionprobe.cpp`, I confirmed exactly one `InjectTestInput` call
(line ~300) carrying exactly one ordered
Meta-down/N-down/N-up/Meta-up batch (lines 248-251), no `QThread::msleep`
anywhere in the injection path (the one `msleep(250)` at the end of `main()`
is an unrelated `/proc`-snapshot keepalive, not part of interaction). The
observer and the last synchronous `shellBefore` sample are both taken
immediately before that single batch; `shellBefore`'s authenticated owner is
threaded into `openedShellExpectation.requiredUniqueOwner`, so the post-input
`shellAfter` sample is invalidated (`owner-replaced`) if the owner changed
across the batch. The post-input surface match additionally requires
`processId == openedShellExpectation.dockProcessId` and exact current/desired
output equality — this was a loosening in the pre-candidate code
(`targetOutput.isEmpty() || ...`) that the diff tightens to always-exact.

**4. Python consumers, mutation coverage, CMake, module cohesion.**
`desktop_session_notification_shell.py` independently re-derives dock/output
authority from the same JSON and cross-validates the raw `notificationShell`
field against it with the identical Pending/Invalid boundary (ambiguous PID
raises `RuntimeError` before the unsettled-pending path can fire). Unit
coverage in `test_desktop_session_readiness_unit.py` explicitly exercises
`multiple-owners`, `malformed-owner` (leading-zero PID string rejected), and
every named pending code including a "not retryable" rejection for the
pre-sample gate codes leaking into the wrong slot. `tst_desktopnotificationbinding.cpp`
gained data rows for `different-component`, `component-unresolved`,
`component-active-query-error`, and `component-inactive`, plus explicit
malformed-activation-sequence rows. CMake registration
(`DesktopNotificationShellReadinessTests.cmake`, new) is additive and
correctly wires the new translation unit into both its own focused test
binary and the existing `qindaqt-desktop-session-probe` target. The new
module has its own header-defined public boundary and is consumed by the probe
only through that header — consistent with AGENTS.md modularity.
`docs/wiki/development/testing-harness.md`'s diff (52 lines) accurately
describes the shipped behavior; I did not find any documentation claim
unsupported by the code.

**5. Runtime evidence (archive inspection).** All six named result
directories exist with `outcome: success`, `returnCode: 0`, `timedOut: false`.
Both 1080p150 runs (`f43ec2903...`, `225db757b...`) show identical
owner `:1.17`/PID `53`, `centerOpenedCount` `0 -> 1`, closed-hidden before and
open-visible after, both anchored to `WL-0` — consistent duplicate passes, not
a single run relabeled twice. The final four-row matrix
(`3f25519ea...` WUXGA, `59a629ea5...` 1440p125, `5f3071bad...` 1080p150,
`f9a6b026c...` dual) has monotonically adjacent start/finish timestamps
consistent with one serial `--parallel 1` invocation totaling ~33.2s,
matching the claimed 33.63s. The dual row's `postSelectorOutputs` carries
`outputGeneration: "2"` (advanced past pre-selector) with ordered authority
`[WL-1 priority 1, WL-0 priority 2]`, and its notification-center surface
maps to `desiredOutputName`/`outputName` `WL-1` only after that selector ran.
I recomputed the dual row's screenshot SHA-256 myself and it matched the
archived full-frame digest exactly, and visually confirmed (via direct image
inspection) that the notification center genuinely renders open in the
correct position on all three single-output screenshots I sampled (dual,
WUXGA, 1440p125), with the correct profile/theme per the S3 table. PSS
(239375 KiB for the dual row) is well under the 1,048,576 KiB ceiling, and
`cleanup.survivorPids` is empty with authenticated terminal phases for every
role. This is archive evidence of prior runs, not something I re-executed.

## P3 findings (nonblocking)

1. **Freshly-introduced duplication.** This same diff adds a byte-identical
   private `_canonical_counter(value, location)` helper independently in both
   `desktop_session_interactive.py` (new in this diff) and
   `desktop_session_notification_shell.py` (new file), differing only in
   exception type/message text. It also adds a third near-duplicate
   `_canonical_process_id` alongside the pre-existing ones in
   `desktop_session_topology.py`/`desktop_session_interactive.py`, with a
   silent bound difference (new code requires PID > 1; the older topology
   helper only requires PID > 0). None of this is a functional defect — each
   copy is internally consistent with its own producer — but it is duplicate
   setup the module boundary should probably share.
2. **Line-count headroom.** The two new files land at 494
   (`desktopnotificationshellreadiness.cpp`) and 499
   (`DesktopSessionTests.cmake`) nonblank lines, just under the 500-line
   decomposition-review threshold. Not a violation today and AGENTS.md's rule
   targets hand-written production source rather than test/CMake
   infrastructure, but worth watching before the next S3 addition lands on
   either file.

## Disposition

No P0/P1/P2 findings. The candidate's fail-closed semantics, single-input
causality, and cross-language evidence validation are internally consistent,
match the normative wiki text exactly, and are corroborated by genuine
(hash-verified, visually-inspected) archived runtime evidence across all six
named result IDs. **ACCEPT for integration.** Remaining boundary is manager
integration and manager-tree replay, as the implementer's handoff already
states.
