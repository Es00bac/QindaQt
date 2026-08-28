# ADR-0043: Isolate the customization editor domain as its own module

- **Status:** Accepted
- **Date:** 2026-08-28
- **Owners:** Shell customization / Customize editor
- **Supersedes:** None
- **Superseded by:** None

## Context

The customization architecture (shell-customization thread artifact
`1787922661`) proposes a first C0 slice: a real drag-and-keyboard layout
editor with persistence. Its steps 3–5 place the editor under
`src/apps/settings_center/editor/**`, splitting intent, model, and UI across
the settings application's source tree.

That placement conflicts with durable cohesion. The editor's domain rules —
preview-bracket-per-gesture, converging in-drag
execution, revision chaining, deterministic rollback, atomic user-profile
persistence, pointer/keyboard parity — are engine-adjacent policy that must
be testable without a window, a QML engine, or the settings application. The
transaction engine in `src/shell_customization` deliberately provides no
persistence and no presentation, and the boundary rules forbid re-implementing
placement policy in the editor.

## Decision

Own the editor's domain as a new cohesive, always-built module
`src/shell_customization_editor` with public headers under
`include/qindaqt/shell_customization_editor`, focused tests under
`tests/shell_customization_editor`, and this wiki page as its contract. The
module links only public `shell_customization`, `profiles`, and Qt Core
values. It adds no `EditingCommand` kind, no persisted profile field, no QML,
and no process. The Settings window (a later slice) composes this domain with
its repository, the Settings1 selection commit, and the QML presentation.

User-profile persistence remains solely owned by
`Profiles::UserProfileStore`. The editor has a narrow destination adapter but
owns no filesystem, validation, serialization, or atomic-replacement policy.
The profiles boundary validates each typed value, proves a strict-loader round
trip, and writes atomic `QSaveFile` documents under a user directory. Layered
catalog precedence and the
`--user-profile-dir` shell option remain the profiles and shell lanes' work
and must consume these documents, not duplicate the writer.

Pointer/keyboard parity, preview-bracket-per-gesture (architecture D2), and
converging in-drag execution (D3) are enforced by construction here: both
input paths call one pure translator and one gesture state machine, and the
focused tests assert sequence identity and the rollback rules.

The session retains the constructor's committed profile as its canonical
applied baseline and replaces that value only after a successful atomic Apply.
Dirty state is content-derived: every successful point or drag commit, Undo,
and Redo compares the complete canonical schema-v1 profile to that baseline.
Repository revisions and undo-stack position are ordering mechanisms, not
evidence that the edited content differs from what was applied.

The editor seam distinguishes a readable published snapshot from unique-lease
readiness. Apply and mutation fail closed unless the adapter owns the
coordinator. The seam also exposes the coordinator-retained committed profile
independently of a provisional snapshot, but only to the lease holder. A
session constructed while a foreign preview is visible therefore remains
read-only and fails dirty until normal retry acquires the lease; it adopts the
retained committed value before its first successful edit or Apply. This keeps
losing windows from writing and prevents a later exact Undo from retaining a
false dirty state.

ADR numbers are allocated by the manager's shared registry. This record does
not reserve a subsequent number for the future `org.qindaqt.ShellLayout1`
cross-process protocol decision.

## Consequences

- The settings-center tree gains only composition code in a later slice; the
  reusable editor domain remains independent of any one presentation host.
- The editor domain builds and tests in the dependency-light CI lane; no
  production-shell build option is required.
- A second future consumer (for example a dedicated editor window process)
  can reuse the identical domain without importing settings-center internals.
- Presentation concerns (canvas, outline, inspector, offscreen matrix) are
  explicitly out of scope until a slice owns them; this module's contract is
  the domain boundary they must consume.

## Alternatives considered

- **`src/apps/settings_center/editor/**` (as proposed by the architecture).**
  Rejected: it makes reusable transaction/session policy change with one
  application's presentation and lifecycle. The composition seam stays
  available once the presentation slice lands.
- **Extending `src/shell_customization` with presentation and persistence.**
  Rejected: the module's boundary row forbids persistence and presentation,
  and its engine has one reason to change (transaction policy).
- **A full C0 slice including manifests and `shell_targeting`.** Out of scope
  for this record; those steps belong to the applet-data and shell-layout
  lanes per the architecture's consent list.

The owning behavior is documented in
[Customization editor domain](../shell/customization-editor.md).
