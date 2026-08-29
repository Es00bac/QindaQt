# Nia Hart material finding: expanded installed consumer uses nonexistent properties

- **Timestamp:** 2026-08-28T04:05:20Z
- **To:** Cora Vale, Controls S2 lead/keeper
- **Severity: HIGH — breaks the P2-1 gate as authored (false-RED, not false-green)**

`tests/controls/qml/tst_installed_controls.qml` — the consumer your P2-1
repair expanded and that both the new qmllint tooling step and the runtime
qmltestrunner step consume — assigns four properties that do not exist on the
named components:

- `:23` `labelText: "Profile name"` on `C.FormRow` — the actual property is
  `label` (`src/controls/qml/FormRow.qml:12`).
- `:24` `helperText: "Choose a unique name"` on `C.FormRow` — the actual
  helper-description property is `description` (`FormRow.qml:13`).
- `:47` `title: "Qinda Dark"` on `C.ThemeCard` — the actual property is
  `themeName` (`src/controls/qml/ThemeCard.qml:12`).
- `:49` `selected: true` on `C.ThemeCard` — the actual check state is
  `checked` (autoExclusive AbstractButton, `ThemeCard.qml:63-65`).

Line `:39 title` is a `C.StateCard` assignment and is valid.

Consequences once the lane runs: QML cannot compile `Component`s containing
nonexistent-property assignments, so the components never reach
`Component.Ready`, the runtime `compare(...)` assertions fail, and qmllint
reports the same assignments as errors under `--max-warnings 0`. Both halves
of `qindaqt.controls-installed-import` will fail even though the deployment
repair itself is correct — the lane would burn on a known-broken gate. I found
no other nonexistent-property usage in the file: the Button factory
(`text/available/busy/error/accessibleDescription`) and StateCard factory
(`status/title/message/actionText`) all name real properties.

Nothing has run yet, so no gate result is misreported today; this is an
authored-defect report against the unexecuted repair. Suggested correction is
yours to apply: `label`/`description` on FormRow, `themeName` and (if wanted)
`checked: true` on ThemeCard.

Continuing the consolidated audit; terminal handoff follows shortly.
