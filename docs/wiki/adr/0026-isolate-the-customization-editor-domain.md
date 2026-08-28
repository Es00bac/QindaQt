# ADR-0026: Isolate the customization editor domain as its own module

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

That placement collides with two constraints. First, the settings-center tree
is under active additive change by the Appearance Settings lane, so editor
sources interleaved there would couple two lanes' merges. Second, the
editor's domain rules — preview-bracket-per-gesture, converging in-drag
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

User-profile persistence lives in the module's `UserProfileStore` as atomic
`QSaveFile` documents under a user directory, written through the profiles
module's public schema and loader. Layered catalog precedence and the
`--user-profile-dir` shell option remain the profiles and shell lanes' work
and must consume these documents, not duplicate the writer.

Pointer/keyboard parity, preview-bracket-per-gesture (architecture D2), and
converging in-drag execution (D3) are enforced by construction here: both
input paths call one pure translator and one gesture state machine, and the
focused tests assert sequence identity and the rollback rules.

The next free ADR number after this record is 0027; the future
`org.qindaqt.ShellLayout1` cross-process protocol ADR from the customization
architecture should take it.

## Consequences

- The settings-center tree gains only composition code in a later slice, so
  the Appearance Settings and editor lanes cannot collide in the same files.
- The editor domain builds and tests in the dependency-light CI lane; no
  production-shell build option is required.
- A second future consumer (for example a dedicated editor window process)
  can reuse the identical domain without importing settings-center internals.
- Presentation concerns (canvas, outline, inspector, offscreen matrix) are
  explicitly out of scope until a slice owns them; this module's contract is
  the domain boundary they must consume.

## Alternatives considered

- **`src/apps/settings_center/editor/**` (as proposed by the architecture).**
  Rejected for now: shared-tree merge coupling with the active Appearance
  Settings lane, and domain policy entangled with application composition.
  The composition seam stays available once the presentation slice lands.
- **Extending `src/shell_customization` with presentation and persistence.**
  Rejected: the module's boundary row forbids persistence and presentation,
  and its engine has one reason to change (transaction policy).
- **A full C0 slice including manifests and `shell_targeting`.** Out of scope
  for this record; those steps belong to the applet-data and shell-layout
  lanes per the architecture's consent list.

The owning behavior is documented in
[Customization editor domain](../shell/customization-editor.md).
