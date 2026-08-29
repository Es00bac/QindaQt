# Dorian Vale — virtual desktop S0+S1 exact source-review midpoint

- **Timestamp:** 2026-08-28T11:09:04Z
- **Exact candidate:** `fd9faab5ab79017be903dafc6f0587d09c511f49`
- **Tree:** `faf4f4327e7c4e352d28b11ded1d24f5ce518e3a`
- **Parent/base:** `7da3300cbe9a22fda077a07ff94b03b7adad396f`
- **Status:** working; blocking source findings reproduced without runtime

## Exact identity and scope

The detached review worktree exactly matches the assigned commit, tree and
parent. The diff has 20 paths, 2,737 additions, no deletions, no whitespace
errors, and sorted path-manifest SHA-256
`9dc4cae417408377abc7436fc602edc75fb3f6ee4204f777e187db5b43621a0c`.
The worktree is clean. No build or private runtime has been entered.

## Blocking source findings

### P1 — the one-shot probe races the topology it is meant to prove

`desktop_session_runtime.py:242-254` starts Settings and Text Editor, then
immediately starts one probe. `desktopsessionprobe.cpp:103-107` waits only for
four D-Bus names; once those are owned it snapshots `Windows` and
`DevelopmentShellSurfaces` exactly once at `:126-144`. Service ownership does
not imply either application window has mapped or the asynchronously starting
shell has committed its dock. The 250 ms delay at `:149-151` occurs after the
marker is emitted and cannot repair the snapshot. A correct session can
therefore fail nondeterministically, and a later successful state is never
observed. The smallest repair is a bounded readiness loop that repeatedly reads
all topology inputs until the exact validator predicate can succeed, while
failing on method/service error or the deadline.

### P1 — application evidence is synthesized from titles, not observed identity

The public `Windows` record already exposes `applicationId`, but
`desktop_session_runtime.py:149-161` matches only a title substring, then writes
the expected `appId` constant into evidence. The declared
`ApplicationExpectation.process_role` at `desktop_session_topology.py:27-31`
is never consumed, and validation at `:251-268` checks only the manufactured ID,
title and boolean. Any private-session window with a matching title can satisfy
Settings or Text Editor while the intended application's mapping is unproved.
The smallest repair is to retain and validate the compositor-observed exact
`applicationId` (and bind it to the expected role as far as the public
inventory permits), with a negative unit where the title matches but the
reported app ID differs.

### P1 — accepted log/evidence provenance is not preserved

ADR-0026 requires evidence **and logs** to be copied before authenticated root
removal (`docs/wiki/adr/0026-contain-virtual-desktop-qualification.md:79-80`).
`test_desktop_session_nested.py:116-123` copies only the artifact directory and
one combined `sandbox.log`; it never copies `paths.logs`, which owns every
per-process log written by `desktop_session_runtime.py:42-55`. The fixed
`desktop-session-last` directory is also neither reset nor sentinel-
authenticated, so artifacts absent from a later failed run can remain from an
older run. Timeout/teardown exceptions can bypass `_copy_artifacts` entirely
before `finally` deletes the run root (`test_desktop_session_nested.py:151-165`).
The smallest repair is an authenticated per-attempt output directory (or safe
fresh reset), copy both artifacts and process logs in a failure-safe path with
run ID/result metadata, then remove the run root; add stale-file, symlink and
failure-path units.

## Passing boundaries so far

The typed bubblewrap argv is structurally fail-closed: empty root, user/PID/
network/IPC/UTS namespaces, parent death/new session/clearenv, private `/dev`,
`/tmp`, `/run`, and no broad root bind. System/stage/source/probe mounts are
read-only; runtime/log/evidence are the only host-backed writable mounts.
Staged executables/plugins reject final symlinks and escapes; Settings1/Audio1
descriptor `Exec` is recorded but direct exact staged executables are used.
Run/stage deletion is sentinel- and build-parent-gated. Process identity binds
PID/executable/kernel start time and TERM-to-KILL is bounded. CMake discovers
tools and registers the safe unit/package rows separately from a serialized,
manager-token-gated boot row. Files remain modular and below project limits.

The exact live row remains deliberately unrun. D0 output/generation authority
and Notification Live's additional development-only `scope=dock` broadening
must exist in the manager's integrated descendant before any boot attempt; D1
remains an ordering/regression dependency only.
