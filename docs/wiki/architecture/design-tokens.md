# QST-1 semantic design tokens

QindaQt Semantic Tokens revision 1 (QST-1) is the single semantic style
vocabulary for first-party QML. It derives a complete immutable value from a
public schema-v1 `ThemeSpec` and explicit caller inputs. Theme JSON includes
the token fields plus the shell-only `iconTheme` presentation hint documented
in [Theme schema v1](../reference/theme-schema-v1.md); QST-1 neither consumes
that hint nor adds stored token fields.

The durable ownership and framework decision is
[ADR-0013](../adr/0013-own-qst1-semantic-tokens.md).

## Module boundary

`src/design_tokens` contains two collaborators:

- `QindaQt::DesignTokens` is a pure C++20 value/derivation library. It links
  only public `themes` values and Qt Core/Gui. A successful derivation may run
  on any thread and returns a shared immutable value.
- `QindaQt.Tokens 1.0` is the narrow Qt QML adapter. Each `QQmlEngine` owns one
  GUI-thread singleton named `Tokens`; C++ composition publishes complete
  values, while QML can only read properties and observe `tokensChanged`.

The module never discovers a catalog, selects a theme ID, reads Settings1,
persists values, imports shell or application code, or depends on Kirigami.
High-contrast catalog selection is caller policy: a caller requesting the
dedicated Qinda High Contrast palette supplies that already resolved
`ThemeSpec`.

## Public contracts

### Ownership and lifetime

`DesignTokenDeriver::derive()` returns a `shared_ptr<const DesignTokens>`.
Every role is populated before publication; no role has a public mutator. A
theme or input change creates a new value. Copies may safely outlive their
source theme and caller inputs.

`TokenFacade` lives for its QML engine's lifetime. Its C++ `publish()` methods
accept either a valid theme plus inputs or a complete token value. They replace
all cached role maps before emitting one `tokensChanged` signal. Publishing an
identical value succeeds without advancing `generation` or emitting a signal.
Before the first publication, `ready` is false and role maps are empty;
application composition must publish before constructing token-dependent
controls rather than inventing presentation fallbacks.

The production shell and `qindaqt-shell-preview` share one shell-owned
publisher. After catalog selection it imports the engine singleton and
publishes the selected `ThemeSpec` before any panel or hosted-applet QML is
created. Icon-runtime installation follows token publication, so a typed icon
placeholder can always resolve its semantic colors. `ThemeCatalog::currentChanged`
replaces the complete QST generation;
an initial or later publication failure is a process-level error, because a
shell with undefined semantic roles is not a usable fallback. The publisher
does not choose themes or read settings, and its borrowed engine/catalog must
outlive it. [ADR-0071](../adr/0071-publish-and-prove-shell-token-readiness.md)
records the startup and live-evidence contract.

The production shell composes the caller inputs below from confirmed Settings1
preferences (`fonts.family`, `fonts.pointSize` and the `accessibility.*` scope)
at startup and on each later confirmed snapshot; owner loss retains the last
confirmed values and malformed snapshots are dropped with a diagnostic
([ADR-0074](../adr/0074-compose-shell-preferences-through-settings1.md)). The
`appearance.theme` preference selects the published theme unless an explicit
`--theme` locks it, and `fonts.family` overlays the selected theme's family in
the derived `type` map without touching the mono family. The publisher itself
still has no settings dependency: the shell runtime projects validated values
into `AccessibilityInputs` and the family overlay. The same confirmed change
also reaches the raw panel and notification `theme` maps: the panel window
factory and the notification window controller update their cached map for
future windows and push the new map onto every live window's plain `theme`
property in the same step.

`appearance.colorScheme=system` preserves an installed requested theme exactly.
Only explicit `light` or `dark` preferences select a compatible variant. This
keeps a user's valid Qinda theme authoritative when the host Qt palette differs
from the Qinda session palette.

### Threading and errors

Pure derivation is thread-neutral and has no ambient state. A loader-valid
schema-v1 `ThemeSpec` always yields a complete QST-1 value. Directly constructed
invalid values return one typed `DerivationError` and diagnostic, with no
partial result. Numeric caller inputs are total: non-finite values become the
documented defaults and finite values clamp to the public bounds below.

Facade publication is GUI-thread confined. A null value, invalid theme, or
wrong-thread call returns `false`, optionally fills an error string, preserves
the last confirmed generation, and emits no signal. Publication is not exposed
through `Q_INVOKABLE`, so imported QML has no theme-selection or settings
authority.

### Compatibility

QST revision is the constant `1`, exported to C++ and QML. The property names
and meanings in this page are the compatibility boundary for
`QindaQt.Tokens 1.0`. Renaming/removing a role or changing its semantic meaning
requires a QST and QML-module revision. A compatible correction to contrast or
rounding stays in QST-1 and requires built-in-theme and property tests. A role
requiring new authored theme data requires a separately reviewed theme-schema
revision and migration; derived roles do not.

## Caller inputs

| Input | Normalization | Meaning |
| --- | --- | --- |
| `basePointSize` | default 10; clamp 6–72 points | Caller-owned font preference; intentionally not theme data |
| `textScale` | default 1.0; clamp 0.5–3.0 | Accessibility multiplier applied to the whole type ramp |
| `reducedMotion` | Boolean | Caps non-instant durations at 80 ms |
| `reducedTransparency` | Boolean | Flattens every semantic color to an opaque palette and disables background blur/shadows |
| `highContrast` | Boolean | Uses theme text for focus and strong outlines; theme selection remains caller-owned |

The later Settings Center/application composition layer may project validated
settings values into this struct; the production shell does so from the
confirmed Settings1 scope named above. That consumer does not authorize a
settings dependency in the token module.

## QST-1 role table

All dimensions are desktop-logical units; physical-pixel conversion belongs to
the rendering/output boundary.

| QML group | Roles | Derivation |
| --- | --- | --- |
| `bg` | `base`, `raised`, `highest` | `canvas`, `surface`, `surfaceRaised` |
| `fg` | `default`, `muted`, `disabled` | `text`, `textMuted`, and `textMuted` at 50% alpha |
| `accent` | `default`, `fg`, `subtle` | `accent`, `accentText`, and `accent` at 12% alpha |
| `state` | `hover`, `pressed` | `text` at 8% and 16% alpha |
| `focus` | `ring` | `accent`, falling back to `text` when accent is below 3:1 on `surface`; high contrast always uses `text` |
| `outline` | `divider`, `strong` | `border`; then border mixed 10% toward text, falling back to text below 3:1 on `surface` |
| `status` | `success`, `warning`, `info` | Fixed light/dark-background pairs with a computed black/white foreground |
| `danger` | `default`, `fg` | `danger` plus the higher-contrast black/white foreground |
| `radius` | `s`, `m`, `l` | `cornerRadius / 2`, `cornerRadius`, `min(32, cornerRadius × 1.5)` |
| `space` | `1`…`6` | Fixed 2, 4, 8, 12, 16, 24 logical-pixel grid |
| `type` | families plus five sizes | Theme families; base × 0.85, 1.0, 1.25, 1.5, and 2.0 after text scale |
| `motion` | `instant`, `short`, `base`, `long` | 0; `max(80, base × 0.6)`; base; base × 1.75, in milliseconds |
| `elevation` | `1`…`3` | Three bounded offset/opacity levels; reduced transparency removes shadow opacity and background blur |
| `accessibility` | `reducedMotion`, `reducedTransparency`, `highContrast` | Read-only projection of the normalized caller inputs for presentation choices that cannot be expressed by a color role; it adds no settings authority to QML |

Default alpha roles remain overlays so controls can apply them to their
semantic surfaces.

### Reduced-transparency flattening

Schema v1 permits alpha in every authored Qt color. When reduced transparency
is requested, QST-1—not a control, shell, or Settings consumer—applies this
ordered transform before deriving the role table:

1. Choose an opaque desktop backdrop from the authored canvas RGB: black when
   its WCAG relative luminance is below 0.5, otherwise white. Canvas alpha does
   not affect this choice.
2. Composite canvas over that backdrop, surface over the flattened canvas, and
   `surfaceRaised` over the flattened surface.
3. Composite border, text, muted text, accent, and danger over the flattened
   surface. Composite accent text over the flattened accent.
4. Derive disabled, subtle, hover, and pressed overlays from that already
   opaque palette and composite each over the flattened surface. Derive focus,
   outlines, status, and danger foregrounds from the same opaque colors.
5. Disable elevation background blur and set shadow opacity to zero.

Every published QST color consequently has alpha 255. Fully opaque authored
themes retain their exact source colors. The fixed threshold, stacking order,
and source-over arithmetic are QST-1 compatibility behavior; changing one
requires exact loader-backed property tests and review as a compatible QST-1
correction or a token revision.

## WCAG pair scope

The built-in-data gate covers exactly QindaQt Pearl, QindaQt Velvet,
QindaQt Smoked Plum, Qinda High Contrast, and Qinda macOS. QST-1 uses the
WCAG 2.2 contrast algorithm and requires:

| Foreground/background pair | Minimum |
| --- | --- |
| `fg.default` on each `bg` level | 4.5:1 |
| `fg.muted` on each `bg` level | 4.5:1 |
| `accent.fg` on `accent.default` | 4.5:1 |
| `danger.fg` on `danger.default` | 4.5:1 |
| Every status foreground/background pair | 4.5:1 |
| `focus.ring` and `outline.strong` on `bg.raised` | 3:1 |
| High Contrast `fg.default` on `bg.raised` | 7:1 |

`fg.muted` remains visibly secondary but is safe for text and essential state
glyphs on every standard surface. Disabled colors are exempt inactive UI, alpha state roles
are overlays rather than standalone content, and `divider` is decorative. A
later control may introduce another semantic pairing only with its own
computational contrast gate.

## Verification and performance evidence

The focused selectors are:

```sh
ctest --test-dir build/dev \
  -R '^qindaqt\.design-tokens-' --output-on-failure
```

They cover schema/metric boundaries, deterministic input normalization,
loader-valid translucent-theme flattening with exact values, complete QML
maps, exact five-theme WCAG pairs, GUI-thread ownership, atomic change
publication, same-value suppression, and a clean staged-install C++ consumer
that recompiles against installed headers/libraries and verifies exact role
keys plus representative Qinda macOS accessibility values.
The offscreen singleton test uses Qt's software renderer and never opens or
controls a desktop surface.

`qindaqt.design-tokens-benchmark` derives 1,000 all-five-built-in batches per
timed iteration so sub-millisecond work remains visible in QtTest's stable
reporter. Candidate handoffs divide the reported time by 1,000 and record the
median against the target of less than 1 ms for one five-theme batch. CI
deliberately has no absolute wall-clock assertion because shared-runner jitter
would make that gate unstable; reviewers compare recorded measurements and
investigate material regressions. On the repaired S1 candidate host,
20-iteration medians were 28.5 ms Debug and 10.8 ms Release per 1,000 batches,
or 0.0285 ms and 0.0108 ms per complete five-theme batch. This is
derivation evidence only—not a Settings Center startup, repaint, memory, live
accessibility bridge, or physical-display claim.

## First-party readability composition

AppAppearance exposes confirmed `accessibilityInputs()` and overlays confirmed
font families on its resolved theme. Consumers explicitly subscribe to
`fonts.family`, `fonts.monospaceFamily`, `fonts.pointSize`, and
`accessibility.textScale`, `highContrast`, `reducedMotion`, `reducedTransparency`.
QML `publishTokens()` consumes those values; Widgets adapters accept the same
inputs. Missing optional keys and owner loss retain the prior confirmed values;
malformed values do not replace them. An explicit theme override locks the
palette, while user readability preferences still apply.

## Standard Qt consumers

The [Qt platform theme](qt-platform-theme.md) projects the same QST values into
standard `QPalette`, interface/fixed `QFont` and icon hints for ordinary Qt
applications. Its private QPA adapter observes AppAppearance; token derivation
retains its pure public boundary. Existing first-party skins and layouts remain
unchanged. The desktop defaults to built-in Qt Fusion rendering for standard
controls, preserves explicit toolkit overrides, and never writes KDE appearance
configuration ([ADR-0115](../adr/0115-share-appearance-through-qt-platform-theme.md)).
