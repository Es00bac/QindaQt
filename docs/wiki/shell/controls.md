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
| `Button` | Primary, secondary, destructive, error, disabled, and busy actions. `available` is the caller-owned capability/enabled input; effective inherited `enabled` is `available && !busy`, so busy always suppresses pointer, keyboard, and accessible activation without losing caller intent. Disabled buttons use the muted foreground on the raised surface instead of retaining an accent or danger fill whose foreground pairing no longer applies. |
| `Label` | Body text with normal/muted and enabled/disabled semantic foregrounds. |
| `TextField` | Editable text with semantic selection, placeholder, focus, disabled, and error presentation. |
| `ComboBox` | Tokenized selector with a raised closed surface, text, indicator, focus, disabled state, and popup rows. The popup reuses the root delegate model, keeps Qt Quick Controls keyboard/type-ahead behavior, and exposes an on-demand vertical scrollbar for long catalogs. When `editable` is true, the closed content is a tokenized editable text field bound to `editText`, including real key input, selection, validator, input-method, and accepted behavior; the wrapper owns no model or selection policy. |
| `CheckBox` | Native check behavior with token-rendered indicator and full Space-key semantics. |
| `Switch` | Native check behavior whose knob position derives from `visualPosition`, so checked/unchecked truth mirrors in RTL. |
| `Slider` | Native range/value behavior. The handle follows `visualPosition`; the progress extent uses the logical value while its origin follows the effective leading edge in RTL. |
| `FormSurface` | Raised, grouped form background with QST spacing and outline. |
| `SectionHeader` | Wrapping section title and description with one accessible text node. It uses the subtitle rung of the shared type scale; a route owns the one prominent page title, and supporting descriptions use the muted foreground. |
| `FormRow` | Responsive label/helper/error plus editor host. `editor` is required and must be declared inside the row (or explicitly reparented into it): the association does not reparent an inline property object. While associated, the row's required label and current helper/error description intentionally supersede the editor's own `accessibleName` and `accessibleDescription`; its native role and value interface remain intact. |
| `StateCard` | Information, success, warning, error, and busy presentation with an optional ordinary action. Its raised neutral surface reserves semantic color for a narrow leading rule, so status does not resemble a competing primary action. Warning and error expose `AlertMessage`. Post-construction changes to status, title, or message coalesce through the next event turn, then Qt's accessibility API announces exactly one complete latest status/title/message tuple (assertive for warning/error, polite otherwise). Construction does not announce, and readiness/revision bookkeeping is private. The read-only `politeAnnouncement` and `assertiveAnnouncement` properties expose the exact Qt/QML values rather than assuming they match a C++ enum representation; `accessibilityAnnouncementRequested` mirrors the exact published tuple for deterministic offscreen verification, not as a substitute AT bridge. |
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

Panel applets keep their accessible identity on the interactive control while
their compact content is icon-first. Icon-only controls use the shell icon
module's typed placeholder when unresolved; they do not restore text inside
the panel. Detail text and operational notices belong in focusable popups.
The compact contract is 28 logical pixels high with token-derived spacing;
vertical panels suppress task and percentage labels rather than clipping them.

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
the visual harness. The fixture registers the Noto Sans Regular/SemiBold/Bold
and Noto Sans Mono Regular files vendored under `tests/controls/fonts/` —
Copyright 2022 The Noto Project Authors, SIL Open Font License 1.1, with the
license text stored beside the fonts, and with valid OpenType table checksums
and `head.checkSumAdjustment` enforced by the pinning row — through
`QFontDatabase::addApplicationFont`, verifies each registration, and rewrites
every theme catalog family (`Inter`, `JetBrains Mono`, `Noto Sans`, `Noto
Sans Mono`, and anything else `data/themes/*.json` names) onto the registered
families through pinned runtime theme copies plus `QFont` substitutions,
because a substitution cannot redirect a family the host actually has
installed. It fixes the C locale and fails closed when a vendored file is
missing, unreadable, renamed, or checksum-invalid. The vendored name records
declare the repository-owned families `QindaQt Sans` and `QindaQt Sans Mono`,
which no host-installed font can declare, so the fixture cannot collide with
or be shadowed by host Noto however Qt or fontconfig order their matches; a
host Noto package update cannot change the rendered glyph bytes. The row
environment keeps the documented host fontconfig configuration because an
empty configuration re-wraps text and removes the fallback glyph the
baselines contain; the dedicated
`qindaqt.controls-visual-no-noto-100-qinda-high-contrast-compact` canary row
reruns the previously bypassing high-contrast row under the checked-in
host-Noto-hidden fontconfig described in the
[testing harness](../development/testing-harness.md#current-reusable-controls-proof).
Qt's software backend
renders every row and intentional baseline changes are stored in
`tests/controls/baselines` for review; changing the vendored font files is a
reviewed baseline regeneration under
[ADR-0021](../adr/0021-isolate-controls-visual-rows.md).

A static gate rejects built-in theme IDs, `sourceThemeId`, palette hex literals,
and every production QML import outside `QtQuick`, `QtQuick.Controls`,
`QtQuick.Layouts`, and `QindaQt.Tokens`. A clean staged-install test requires
the exact 15 Qt-generated QML deploy paths, runs a strict tooling consumer and
the compiled runtime import from the staged QML root, and clears ambient source,
build, and QML import paths. The Controls backing library resolves the sibling
installed `QindaQt/Tokens` backing library through a relative runpath, so
relocating the prefix does not depend on host library paths. The memory gate
runs matched bare-Qt-Quick and token-plus-controls offscreen processes, reads
five `smaps_rollup` PSS samples from each exact PID over three pairs, and records
the median delta without inventing a machine-independent threshold.

Controls never self-publish tokens. Every QML composition root must publish a
complete engine-owned facade before constructing a Control; the production
shell and preview now enforce this before their panel dispatchers and exit on
publication failure. A warning-clean dispatcher row runs with
`QT_FATAL_WARNINGS=1`, so an undefined token assignment is a startup regression
rather than an accepted visual fallback.

The focused selector currently discovers 34 tests: one behavior gate, 25
process-isolated visual rows, the host-Noto-hidden canary row, font pinning,
three font-fixture fail-closed controls, source policy, staged installed
import, and PSS measurement.

These are compiled QML, software-renderer, packaging, and process-memory
checks. They do not qualify live assistive technology, compositor focus,
physical DPI/output behavior, GPU rendering, application navigation, Settings1
composition, service availability, or a complete Settings Center.

## Application material and icon primitives

`Icon` presents a bounded named icon from the public application catalog seam;
set `name`, `width`/`height`, and optional semantic `color`. It preserves aspect
ratio and tints alpha when a color is supplied. The containing command supplies
its accessible name; decorative icons are ignored by accessibility.

`MaterialSurface` is a rounded semantic rectangle with a `raised` Boolean. It
adds local translucent highlights over its opaque background. High contrast
and reduced transparency remove those highlights. It does not request backdrop
capture or compositor blur. See [Icon theme](icon-theme.md) and
[ADR-0109](../adr/0109-use-pearl-and-smoked-plum-app-materials.md).
