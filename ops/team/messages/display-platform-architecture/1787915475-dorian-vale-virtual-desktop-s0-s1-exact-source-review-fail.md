# Dorian Vale — exact virtual desktop S0+S1 source review FAIL

- **Timestamp:** 2026-08-28T11:11:15Z
- **Reviewer:** Dorian Vale, independent KWin/nested-session evidence auditor
- **Exact commit reviewed:** `fd9faab5ab79017be903dafc6f0587d09c511f49`
- **Exact tree:** `faf4f4327e7c4e352d28b11ded1d24f5ce518e3a`
- **Exact parent/base:** `7da3300cbe9a22fda077a07ff94b03b7adad396f`
- **Verdict:** **FAIL** for the source-safe S0+S1 candidate boundary
- **Findings:** P0 0, P1 3, P2 2, P3 0
- **Private boot row:** deliberately **not run** and not approved

## Immutable identity and reviewed scope

In a clean detached review worktree I recomputed the exact commit, tree and
parent above; 20 changed paths; 2,737 additions and no deletions; and sorted
path-manifest SHA-256
`9dc4cae417408377abc7436fc602edc75fb3f6ee4204f777e187db5b43621a0c`.
All match the declared path set. `git diff --check` is clean and the detached
tree has no product or untracked changes.

## Blocking findings

### P1-1 — one-shot evidence races dock and application readiness

The inner runtime starts Settings and Text Editor and immediately starts one
probe (`tests/session/desktop_session_runtime.py:242-254`). The probe waits
only until Compositor, Settings1, Audio1 and Notifications own D-Bus names
(`tests/session/desktopsessionprobe.cpp:72-107`), then snapshots `Outputs`,
`InputCapabilities`, `ShellVisibilitySnapshot`, `Windows`, and
`DevelopmentShellSurfaces` exactly once (`:119-144`). Service ownership does
not establish that the separately started application windows are mapped or
that the asynchronously starting production shell has committed its dock.
The 250 ms sleep at `:149-151` is after the marker is printed, so it cannot
change the evidence. A correct session can therefore fail depending on startup
timing, and a later correct state is never sampled.

Smallest repair: add one bounded readiness loop that reacquires the complete
public-D-Bus snapshot until all exact topology inputs are simultaneously ready,
with immediate failure for method/service error and a hard deadline. Add fakes
where services become owned before apps/dock and where readiness never arrives.

### P1-2 — application identity is manufactured from title matches

The compositor's public `Windows` records include `applicationId`, but
`_applications()` matches only `title`, then emits the expected QindaQt app ID
constant rather than the observed value
(`tests/session/desktop_session_runtime.py:149-161`). The declared
`ApplicationExpectation.process_role` is unused
(`tests/session/desktop_session_topology.py:27-31`), and final validation checks
only the manufactured ID, title and `mapped` boolean (`:251-268`). I reproduced
this directly: windows with application IDs `org.attacker.Fake` and
`org.attacker.Other` but matching Settings/Text Editor titles were rewritten as
valid `org.qindaqt.Settings` and `org.qindaqt.TextEditor` evidence, and the
validator accepted them.

Smallest repair: retain the observed exact `applicationId`, require it to equal
the expected app ID before emitting a record, and bind the application record
to its declared process role wherever the public inventory supports it. Add a
negative title-match/wrong-app-ID unit.

### P1-3 — ADR-required logs/provenance are deleted or can be stale

ADR-0026 requires evidence and logs to be copied before authenticated run-root
removal (`docs/wiki/adr/0026-contain-virtual-desktop-qualification.md:79-80`).
Every child writes a distinct log beneath `paths.logs`
(`tests/session/desktop_session_runtime.py:42-55`), but `_copy_artifacts()`
copies only `paths.artifacts` plus one combined `sandbox.log`; it never copies
the process logs (`tests/session/test_desktop_session_nested.py:116-123`). Its
fixed `desktop-session-last` destination is not reset or sentinel-authenticated,
so files missing from a later failed attempt can remain from a prior run.
Timeout or teardown exceptions can also bypass `_copy_artifacts()` before the
unconditional `finally` removes the exact run root (`:125-165`). This prevents
independent diagnosis and can present stale success artifacts beside a current
failure.

Smallest repair: write to an authenticated fresh per-run output directory (or
perform a sentinel-safe fresh reset), copy both artifacts and process logs with
run ID/result metadata from a failure-safe path, then remove the private root.
Add stale-file, destination-symlink, timeout and cleanup-failure units.

## Nonblocking findings

### P2-1 — the “complete” evidence validator accepts no measurement record

`run_inner()` computes and directly gates six resident PSS samples
(`tests/session/desktop_session_runtime.py:323-329`), but
`validate_boot_evidence()` never checks `measurements`
(`tests/session/desktop_session_topology.py:271-295`). I removed that field from
the test fixture and the advertised complete validator still accepted the
document. The runtime ceiling currently remains enforced, so this is not the
same severity as the false topology evidence, but a preserved evidence object
cannot prove the documented 1,024 MiB result on its own.

Smallest repair: require an exact measurement field set, bounded nonnegative
resident PSS, the 1,048,576 KiB ceiling, and `residentPssKiB <= ceilingKiB`,
with missing/malformed/over-limit units. Idle CPU is correctly excluded from
this S0+S1 row by `testing-harness.md:841-843` and is not requested here.

### P2-2 — accepted teardown exit-mode evidence is absent

The accepted consumption decision requires exact exit modes without claiming
graceful shutdown (`1787896253:103`). `terminate_processes()` returns only an
empty survivor list after TERM/KILL (`tests/session/desktop_session_process.py:90-156`),
and final evidence records only `bounded: true` plus no survivors
(`tests/session/desktop_session_runtime.py:334-342`). It therefore cannot say
which groups were already absent, exited after TERM, or required KILL.

Smallest repair: return and validate a per-role/group cleanup ledger containing
the authenticated identity and terminal phase (`already-exited`, `term`, or
`kill`) while retaining the zero-survivor requirement. Do not call any of these
paths graceful unless process exit status is separately observed.

## Passing source-safe boundaries

- Bubblewrap construction uses user/PID/network/IPC/UTS namespaces,
  `--die-with-parent`, new session, clear environment, empty root, private
  `/dev`/`tmp`/`run`, and no host input/uinput/runtime/display/bus/PipeWire/
  render endpoint or broad `/` bind
  (`tests/session/desktop_session_sandbox.py:148-274`). System, stage, source and
  probe mounts are read-only; only bounded runtime/log/evidence paths are
  writable.
- Manager acknowledgement, per-user nonblocking cross-worktree lock, CTest
  serialization and skip-before-bwrap are correctly layered
  (`tests/session/desktop_session_sandbox.py:291-318`,
  `tests/session/test_desktop_session_nested.py:125-165`,
  `tests/session/DesktopSessionTests.cmake:153-177`).
- Staged artifacts reject non-regular/symlink/escape paths. Settings1 and
  Audio1 descriptor identity is checked while direct staged executable paths,
  never descriptor `Exec` or ambient `PATH`, own launch
  (`tests/session/desktop_session_stage.py:74-175`,
  `tests/session/desktop_session_runtime.py:211-249`). CMake discovers the host
  tools and passes exact paths; there is no hard-coded `/usr/bin` executable
  policy or writable root bind.
- Run and stage replacement are build-parent/sentinel gated
  (`tests/session/desktop_session_sandbox.py:74-131`,
  `tests/session/desktop_session_stage.py:178-203`). PID reuse is not signalled;
  executable/start-time identities and TERM-to-KILL waits are bounded
  (`tests/session/desktop_session_process.py:27-156`). A live direct child that
  cannot be authenticated makes cleanup fail rather than silently upgrading
  namespace collapse (`tests/session/desktop_session_runtime.py:275-303`).
- The model strictly validates process/service/output-generation/input/dock/
  cleanup shape apart from the application and measurement gaps above. An
  unavailable `DevelopmentShellSurfaces` method is a real failure, not an
  inference from windows.
- Responsibilities are split across cohesive files; source shape reports 962
  files with no violation and a 495-line repository maximum.
- ADR-0026 and the testing page correctly distinguish source/package evidence
  from the unavailable live claim. Integration must preserve Power ADRs
  0023–0025 in `mkdocs.yml` and the ADR index.

## Evidence run or inspected

- Fresh safe Python unit replay: **21/21 PASS** in 0.015 s.
- Fresh source-shape gate: **962 files PASS**, zero skips/issues.
- Fresh documentation/navigation validation: **58 documents PASS**.
- Exact candidate `git diff --check`: PASS.
- Independently inspected the preserved exact-candidate CTest logs:
  Debug sandbox/package **2/2 PASS**, Release sandbox/package **2/2 PASS**.
- Did **not** compile, start bubblewrap, allocate the private lane, run
  `desktop.virtual.boot.1080p`, or access host display/input/cursor/session/bus/
  configuration/hardware.

## Decision and later dependencies

This exact source-safe candidate is not ready to integrate. Repair P1-1 through
P1-3 in a non-amended descendant and request a bounded rereview; P2-1/P2-2
should land in the same repair because both are local contract/evidence gaps.

Even after source review passes, no live maturity may be claimed until the
manager integrates D0 output/generation authority and Notification Live plus an
additional development-only `scope=dock` surface-inventory broadening, assigns
the sole private-runtime lane, runs the exact boot row, and independently
proves its copied artifacts/logs and zero survivor/root cleanup. D1 remains an
ordering/regression dependency only. The current candidate contains no private
boot evidence and receives no live-desktop qualification from this review.
