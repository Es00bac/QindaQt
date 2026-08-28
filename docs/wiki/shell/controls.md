# QindaQt.Controls 1.0

`QindaQt.Controls 1.0` is the reusable presentation vocabulary for first-party
QindaQt shell and application interfaces. It supplies token-styled Qt Quick
Controls primitives plus the small Qinda-specific components needed for forms,
state, degraded capability, theme choice, and semantic-color presentation.

The module imports [QST-1](../architecture/design-tokens.md) as its only
palette, typography, spacing, radius, motion, elevation, contrast, and
accessibility-transform authority. It does not select a theme, subscribe to
Settings1, know a theme ID, or import shell, AppShell, LayerShellQt, Kirigami,
services, or application routes.
[Module boundaries](../architecture/module-boundaries.md) records that
dependency direction for repository consumers.

## Consumption and ownership

Consumers use an explicit module alias:

```qml
import QindaQt.Controls 1.0 as Qinda

Qinda.Button {
    text: qsTr("Apply")
}
```

The module wraps Qt Quick Controls rather than installing ambient
`QQuickStyle` process state. This keeps the Qinda presentation boundary visible
in source and lets unrelated third-party controls retain their own supported
style. C++ application composition must publish one complete QST-1 generation
before creating token-dependent controls. Controls observe complete generation
changes; none owns or mutates the token singleton.

QML objects follow their normal visual-parent lifetime and GUI-thread affinity.
They contain no persistence, asynchronous transport, platform object, or
service authority. Their error surface is visual and accessible state supplied
through public properties; business-domain validation and retry policy remain
with the consuming view model.

## Public component set

| Component | Responsibility and notable contract |
| --- | --- |
| `Button` | Primary, secondary, destructive, error, disabled, and busy actions. `available` is the caller-owned capability/enabled input; effective inherited `enabled` is `available && !busy`, so busy always suppresses pointer, keyboard, and accessible activation without losing caller intent. |
| `Label` | Body text with normal/muted and enabled/disabled semantic foregrounds. |
| `TextField` | Editable text with semantic selection, placeholder, focus, disabled, and error presentation. |
| `CheckBox` | Native check behavior with token-rendered indicator and full Space-key semantics. |
| `Switch` | Native check behavior whose knob position derives from `visualPosition`, so checked/unchecked truth mirrors in RTL. |
| `Slider` | Native range/value behavior. The handle follows `visualPosition`; the progress extent uses the logical value while its origin follows the effective leading edge in RTL. |
| `FormSurface` | Raised, grouped form background with QST spacing and outline. |
| `SectionHeader` | Wrapping section title and description with one accessible text node. |
| `FormRow` | Responsive label/helper/error plus editor host. `editor` is required and must be declared inside the row (or explicitly reparented into it): the association does not reparent an inline property object. While associated, the row's required label and current helper/error description intentionally supersede the editor's own `accessibleName` and `accessibleDescription`; its native role and value interface remain intact. |
| `StateCard` | Information, success, warning, error, and busy presentation with an optional ordinary action. Warning and error expose `AlertMessage`; every post-construction semantic transition uses Qt's accessibility announcement API (assertive for warning/error, polite otherwise). The read-only `politeAnnouncement` and `assertiveAnnouncement` properties expose the exact Qt/QML values rather than assuming they match a C++ enum representation; `accessibilityAnnouncementRequested` mirrors the exact tuple for deterministic offscreen verification, not as a substitute AT bridge. |
| `DegradedNotice` | Explicit unavailable capability alert with reason and optional retry action; it never decides whether a service is available. It specializes `StateCard`; consumers may override the generic `title` and use `reason`, `retryText`, and `retryRequested`. Overriding inherited `status`, `message`, or `actionText` would sever the fixed warning and alias bindings and is unsupported. |
| `ThemeCard` | Keyboard-selectable radio choice. `available` is the caller-owned capability input. No supplied preview means the one complete active QST generation. A supplied preview must contain QST-derived `bg.base`, `bg.raised`, `accent.default`, `fg.default`, and `outline.strong` roles whose RGBA components are finite numbers in the Qt color range. Partial, wrong-typed, non-finite, out-of-range, or otherwise hostile maps disable selection and expose one explicit unavailable preview and accessible description; roles never fall back individually into a hybrid of themes. |
| `TokenSwatch` | Named semantic-color sample with a caller description; it does not interpret theme identity. |
| `FocusRing` | Two-logical-pixel QST focus outline bound to one required control. |

`QindaQt.Controls 1.0` names and property meanings form the compatibility
boundary. Removing or renaming a component/property, changing a required
association, or weakening keyboard/accessibility behavior requires a new QML
module revision. Visual corrections that preserve meanings may remain 1.0 when
all theme and baseline gates are reviewed.

`Button` and `ThemeCard` inherit Qt's writable `enabled` property, but
`available` is their only supported caller-owned availability input. Setting or
binding inherited `enabled` directly replaces the component's internal QML
binding and can bypass busy or invalid-preview gating; consumers must not do so.
This is a documented QML usage contract rather than a second state authority.

## Accessibility, localization, and direction

Every interactive primitive keeps Qt Quick Controls' native keyboard behavior
and strong focus policy. Focus is rendered from `focus.ring`; no component
removes the keyboard outline. Buttons, theme choices, check controls, editable
text, sliders, grouped rows, alerts, and static text expose explicit accessible
names, descriptions, roles, and state. A color change alone never conveys
busy, error, required, degraded, checked, selected, or disabled meaning.

Public default strings use `qsTr()` with placeholder-aware complete phrases.
Wrapping labels and helper/error text expand vertically under localization.
Layouts inherit the consumer's `LayoutMirroring`; indicator and progress
geometry uses Qt's logical/visual position contract rather than hard-coded
left/right assumptions. Qinda macOS container-tab direction is a separate
decoration rule and is not reproduced in application controls.

Reduced motion and transparency are already total QST-1 transforms. Controls
read `motion.short` and the published opaque colors directly; they do not add
local timing, alpha flattening, backdrop, or theme-specific branches.

## Qualification boundary

The focused selector is:

```sh
ctest --test-dir build/dev -R '^qindaqt\.controls-' --output-on-failure
```

The behavior gate loads the compiled module with an offscreen software
renderer, publishes each of the five built-in themes, queries Qt accessible
interfaces, and exercises keyboard activation, disabled/busy/error/degraded
state, dynamic alert announcements, required/error editor association, hostile
theme previews, long localized text, RTL switch/slider geometry, and reduced
motion/transparency.

Reviewed image fixtures cover all five themes at compact, ordinary, and large
logical widths at 100%, plus all five ordinary-width rows at 125% and 150%.
Under [ADR-0021](../adr/0021-isolate-controls-visual-rows.md), each of those 25
named CTest rows launches one fresh process and exactly one validated QtTest
data selector. Missing or scale-incompatible selectors fail before execution;
the wrapper also requires the requested row to be the sole tagged visual pass.
Each row verifies its actual device-pixel ratio and captured pixel dimensions
before comparison and waits through the gallery control's published QST motion
duration before requesting reviewed frames. The reduced-motion behavior row
separately proves the transformed duration rather than overriding animation in
the visual harness. The fixture substitutes the schema's
`Inter` and `JetBrains Mono` family names with pinned Noto Sans families,
fixes the C locale, uses Qt's software backend, and stores intentional baseline
changes in `tests/controls/baselines` for review.

A static gate rejects built-in theme IDs, `sourceThemeId`, palette hex literals,
and forbidden imports in production control QML. A clean staged-install test
imports the generated module from its installation prefix with ambient QML
paths cleared. The Controls backing library resolves the sibling installed
`QindaQt/Tokens` backing library through a relative runpath, so relocating the
prefix does not depend on host library paths. The memory gate
runs matched bare-Qt-Quick and token-plus-controls offscreen processes, reads
five `smaps_rollup` PSS samples from each exact PID over three pairs, and records
the median delta without inventing a machine-independent threshold.

The focused selector currently discovers 29 tests: one behavior gate, 25
process-isolated visual rows, source policy, staged installed import, and PSS
measurement.

These are compiled QML, software-renderer, packaging, and process-memory
checks. They do not qualify live assistive technology, compositor focus,
physical DPI/output behavior, GPU rendering, application navigation, Settings1
composition, service availability, or a complete Settings Center.
