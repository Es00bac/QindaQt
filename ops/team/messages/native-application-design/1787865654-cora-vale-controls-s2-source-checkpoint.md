# Cora Vale Controls S2 source-complete checkpoint

- **Timestamp:** 2026-08-27T21:20:54Z
- **Branch/worktree:** `worker/controls-s2` at
  `/home/cabewse/work_SPaC3/container-wm-workers/controls-s2`
- **Exact base/HEAD:** `a083a20af14a2d7b9e954735a2d659c475a536b2`
- **State:** working; uncommitted candidate source authored, compilation held
  by manager for host resources

The source candidate now defines 14 compiled-QML public components in
`QindaQt.Controls 1.0`: Button, Label, TextField, CheckBox, Switch, Slider,
FormSurface, SectionHeader, FormRow, StateCard, DegradedNotice, ThemeCard,
TokenSwatch, and FocusRing. QST-1 is the only palette, spacing, typography,
radius, motion, focus, and accessibility-transform authority; production QML
contains no theme-ID branches, palette hex literals, Settings1, shell,
AppShell, service, LayerShellQt, or Kirigami imports.

Acceptance-sensitive contracts are represented in source and direct tests:

- Button's caller input is `available`; busy derives effective disabled state
  and cannot activate. StateCard child actions use that same contract.
- FormRow requires an `editor` and forwards label/required plus current
  helper/error text to that editor's accessible object without replacing its
  native role/value interface.
- Switch and Slider derive geometry from `visualPosition`; RTL tests prove
  low/high knob and leading-edge fill truth.
- ThemeCard treats a supplied preview as an all-or-nothing map for every role
  it renders; partial/hostile previews become one explicit disabled unavailable
  state and never mix with the active generation.
- StateCard exposes Warning/Error as alerts, Information/Success/Busy as
  non-alerts, includes Busy in static accessible text, and sends the same full
  status/title/message/politeness tuple to Qt's announcement API and a
  deterministic test signal on every post-construction semantic transition.
- Direct behavior/accessibility/property assertions exist for all 14 public
  components, including disabled, busy, error, degraded, keyboard activation,
  focus-ring visibility, RTL, long localization, and reduced transforms.

Definitions also exist for the 25-row reviewed-baseline matrix (five themes at
compact/ordinary/large 100%, plus ordinary 125%/150% with actual DPR and pixel
dimension checks), static source policy, clean installed import, and three-pair
matched bare-Qt-Quick versus controls PSS measurement. The primary controls
wiki page and module/testing/nav registries describe the current public and
qualification boundaries.

No configure, compile, lint, CTest, baseline generation/review, install, PSS,
or docs gate is claimed at this checkpoint. Those remain pending the manager's
explicit release of the build lane; full visual and broad matrices still
require coordination. No commit exists yet.
