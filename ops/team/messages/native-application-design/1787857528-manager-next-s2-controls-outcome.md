# Manager next native-app outcome: QindaQt.Controls S2

- **Timestamp:** 2026-08-27T13:05:28-06:00
- **From:** Manager
- **To:** future Controls implementer and exact-commit reviewer
- **State:** queued after accepted QST-1 integration; no implementation or
  liveness is claimed
- **Base:** the future exact integrated QST-1 milestone commit
- **Owning design:**
  [Juno Park handoff](1787853515-juno-park-design-handoff.md), especially
  sections 2, 4, 7–12

## User-visible outcome

First-party QindaQt applications can build accessible, keyboard-complete forms,
cards, notices, theme choices, and ordinary actions from one compiled
`QindaQt.Controls 1.0` module. Every control resolves QST-1 tokens and visibly
adapts across Qinda Light, Dusk, Dark, High Contrast, and Qinda macOS without
hard-coded per-theme branches.

## Boundary

Own only `src/controls/**`, `tests/controls/**`, its primary wiki page and
baseline fixtures, plus the smallest additive registries. Depend on public
QST-1 and Qt Quick/Controls2/Layouts. Do not import Settings1, shell,
LayerShellQt, Kirigami, appshell, service clients, or app routes. If Qt Quick
Controls style plumbing requires a separate style package, keep that adapter
distinct from QindaQt-specific components and document application bootstrap;
do not create a second palette/token authority.

The first set must cover at least SectionHeader, FormRow, StateCard,
DegradedNotice, ThemeCard, a token swatch, FocusRing, and the styled ordinary
button/text/check/switch/slider/form primitives needed by the next AppShell and
Settings routes. API names may be refined before implementation, but one
component must not accumulate unrelated presentation policies.

## Acceptance evidence

- Offscreen behavior and accessibility-tree/property tests for every public
  component, including enabled/disabled, busy/error/degraded, keyboard
  activation, focus visibility, RTL, long localized text, and reduced
  motion/transparency.
- Deterministic reviewed visual baselines for all five themes at compact,
  ordinary, and large widths, plus 125%/150% pixel-density rows where the
  renderer truthfully applies them. Qinda macOS must retain the mist/sage
  identity without faking application window decorations.
- Source-shape/static checks proving control QML contains no theme IDs and no
  palette hex literals outside deliberate baseline/test data.
- Compiled QML, `all_qmllint`, installed-import consumer, Debug/Release focused
  and broad registries, strict docs/link/source/whitespace, and a measured
  module-plus-token PSS delta against a bare Qt Quick window. Record rather
  than invent any unstable wall-clock threshold.
- Different-worker exact-commit review before integration.

S2 contains controls, not AppShell navigation or Settings pages. Those remain
S3/S4 and must consume this accepted public module rather than copy it.

