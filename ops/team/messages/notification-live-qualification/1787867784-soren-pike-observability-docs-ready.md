# Soren Pike observability and documentation checkpoint

- **Timestamp:** 2026-08-27T15:56:24-06:00
- **From:** Soren Pike, notification live qualification implementer
- **To:** manager and later exact-commit reviewer
- **State:** source/docs ready for focused compile; no nested-run success is
  claimed by this record
- **Base:** `c4982697858c083828bd406f1aa56c4e942bcc10`

## Acceptance gaps closed

The driver now derives each required user-visible claim from authenticated
shell snapshots plus compositor-owned layer-surface state. It asserts unique
popup/center roles, exact shell PID/output/geometry, active center versus
inactive popup, every forward and reverse focus transition while Clear history
and an action card are enabled, disabled/remapped shortcut non-dispatch, DND
suppression/retention/critical bypass/no replay, Settings1 Saving/rejection/
uncertain/outage truth, actual nested KScreenLocker PID ownership, lock-time
resident critical denial, and double-inactive unlock with no popup/history
replay. The external harness requires resident host PID continuity, a distinct
replacement shell PID, and fresh evidence authentication.

ADR-0020 now owns the development-only live-evidence boundary. Compositor1
`DevelopmentShellSurfaces` rejects before KWin inspection in production and
filters development output to the two notification scopes. The shell endpoint
is read-only, has one bounded `Snapshot()`, registers only after exact KWin PID
plus live development capability authentication, and is itself bound by the
harness to the expected shell bus PID. Settings1 is the production path used to
prove visible Saving, confirmed rejection, and uncertain operation; no host
mutation seam was added to manufacture a notification-action error.

## Modular/source changes

The session workflow is split by runtime safety, evidence client, keyboard,
surface, shortcut, lock, Settings phases, installed staging, process/PID
observation, outer private-runtime orchestration, and CLI parsing. Its CMake
registration moved into `tests/session/NotificationLiveTests.cmake`. The QML
focus-cycle/accessibility test moved into its own focused file. The source-shape
gate reports no warning or error.

The new Compositor1 method required small additive coordination edits outside
the initially named session-test path: the checked-in D-Bus XML, its descriptor
parity expectation, and the existing production-control workflow. The latter
now proves `DevelopmentShellSurfaces` returns `control-disabled`. No unrelated
compositor behavior changed.

## Source-only evidence

- `python tools/docs_validation.py --root .` — exit 0; 44 Markdown documents
  and MkDocs navigation validated.
- `python -m tools.source_shape.cli` — exit 0; 796 files, no warnings/errors.
- Python AST parsing for all new notification-live driver modules and unit file
  — exit 0.
- `git diff --check` — exit 0.
- `mkdocs build --strict` — unavailable because `mkdocs` is not installed in
  the current environment; the repository-native link/navigation validator
  passed instead.

Per manager resource coordination I have not started another compiler, broad
registry, sanitizer, installed stage, nested compositor, lock, or race row.
