# Juno Park — native application & design-system design handoff

- **Timestamp:** 2026-08-27T17:58:35Z
- **Worker:** Juno Park — Z.ai via OpenCode, exact `zai-coding-plan/glm-5.3-flash`,
  reasoning variant `high` (reported from this session's raw event stream, not
  inferred from the assignment)
- **Read-only product base:** `dc29c88911f0ed6d381211027f16f46bbf92a07c`
  (verified clean detached checkout; no product path was edited)
- **Claim:** `1787853338-juno-park-claim.md` in this thread

## Scope and evidence legend

This is architecture/design research only. Every statement is tagged:

- **[E]** existing evidence — verifiable at the base commit (path, line, test,
  or wiki page cited).
- **[P]** proposed contract — my design recommendation; not yet accepted or
  implemented. Acceptance requires the wiki/ADR steps in each slice.
- **[M]** missing implementation — required work that does not exist today.

No completion percentage is implied anywhere; this handoff delivers
contracts, not code.

---

## 1. Grounding: what genuinely exists at the base [E]

- **Themes.** `src/themes` loads validated schema-v1 themes (nine required
  color tokens, `decoration` object, `cornerRadius`, `motionDuration`,
  `blurEnabled`, fonts). Five built-ins ship in `data/themes/`, including
  Qinda macOS (left traffic lights, hover glyphs, right-to-left tabs,
  mist/sage palette). Wiki: `docs/wiki/reference/theme-schema-v1.md`.
- **Settings model.** `src/settings` owns schema v1, four-layer resolution
  (session → user → profile → system), optimistic transactions, typed
  `CommitStatus {Applied, ValidationFailed, Conflict, ReadOnlyLayer}`, and
  `SettingsChangeSet` with effective-change records
  (`src/settings/include/qindaqt/settings/settings_types.h`). Domains:
  appearance, fonts, displays, input, panels, window-management,
  accessibility, services (`data/settings/schema-v1.json`). No D-Bus service,
  no settings UI exists at this base; the Settings1 candidate (Ada Ruiz,
  `worker/ada-settings1`) is in late review/repair and is the integration
  front, per the `persistent-notification-quieting` thread.
- **Layout profiles and editing.** `src/profiles` (strict schema v1, ten
  built-ins), `src/shell_layout` (pure collision-free solver, work areas),
  and `src/shell_customization` (exclusive move-only coordinator lease,
  immutable snapshots, typed commands `AddPanel…UpdateAppletSettings`,
  `Undo/Redo/BeginPreview/CommitPreview/CancelPreview`, read-only
  `evaluate()` for acceptance highlighting, manifest-aware placement policy)
  are implemented and tested (`docs/wiki/shell/layout-profiles.md`,
  testing harness selectors `qindaqt.shell-customization-*`).
- **Applet pipeline.** `src/applets` (manifest catalog), `src/applet_host`
  (capability policy), `src/applet_runtime` (five-gate resolution). Live
  built-ins: clock and notification-center entries only; launcher, task-list,
  global-menu, status-tray resolve `implementation-unavailable`.
- **Shell presentation.** `src/shell` (preview + LayerShellQt production
  shell), notification popup/center QML with explicit bidirectional focus
  chains, Escape close, writable accessible DND control (offscreen-proven),
  and shell-owned `Meta+N` via a private KF6 GlobalAccel adapter.
- **The gap this lane fills.** Shell QML consumes themes ad hoc:
  `src/shell/qml/Main.qml:15` reads
  `themeCatalog.current.colors?.canvas ?? "#171a18"` — a `QVariantMap` poke
  with hard-coded fallbacks, repeated across components. There is no
  `src/apps` directory, no reusable control set, no navigation stack, no
  route registry, no design-token module. The First-party-experience
  milestone is Planned (`docs/wiki/development/implementation-roadmap.md`).
- **Toolchain.** Qt 6.11 required (`CMakeLists.txt:46,51`); installed:
  Qt 6.11.1, Kirigami 6.27.0, kirigami-addons 1.12.1, LayerShellQt 6.6.5,
  KDecoration 6.6.5, KF6 6.27.0 (Arch packages).

---

## 2. Native application design principles [P]

1. **Tokens are the only colors.** Application and shell QML never hard-code
   palette values; every visual decision resolves through the semantic token
   layer. This generalizes the theme-schema rule "components consume these
   meanings" (`docs/wiki/reference/theme-schema-v1.md`) to first-party apps.
2. **Apps are ordinary clients.** `src/apps/*` links only public SDK/service
   clients and public QindaQt QML modules — never shell private headers, never
   service implementations (existing rule, `docs/wiki/architecture/module-boundaries.md`).
3. **Honest states over fake features.** A settings control backed by an
   unimplemented service renders an explicit degraded state; it never stages a
   change that cannot be honored, and never pretends success.
4. **Every pointer gesture has a command path.** Existing product constraint;
   the WYSIWYG editor is designed keyboard-first (§6).
5. **Data, not code.** Themes and profiles remain declarative; visual variety
   is achieved by resolving the same components against different tokens.
6. **Latency is a feature.** Startup, page switches, and theme switches have
   budgets (§9) measured in CI, not asserted.
7. **Small modules, explicit contracts.** Each module below states ownership,
   lifetime, threading, error, and accessibility behavior (§4), per root
   AGENTS.md.

## 3. Token system: "QST-1" (QindaQt Semantic Tokens, rev 1) [P]

A deliberately small, two-tier system.

**Tier 1 — theme raw tokens (existing, unchanged):** the nine schema-v1
colors (`canvas, surface, surfaceRaised, border, text, textMuted, accent,
accentText, danger`), `cornerRadius`, `motionDuration`, `blurEnabled`,
`fontFamily`, `monoFontFamily`, `decoration.*`. These stay the only
per-theme data; QST-1 does not require theme schema v2.

**Tier 2 — derived semantic roles (computed, not stored):** a documented,
total derivation from Tier 1 so all five built-ins (and future user themes)
get a consistent richer vocabulary:

| Token | Derivation (initial rule set) |
| --- | --- |
| `bg.base` | `canvas` |
| `bg.raised`, `bg.highest` | `surface`, `surfaceRaised` |
| `fg.default`, `fg.muted`, `fg.disabled` | `text`, `textMuted`, `textMuted` at 50% alpha over `surface` |
| `accent.default`, `accent.fg`, `accent.subtle` | `accent`, `accentText`, `accent` at 12% alpha over `surface` |
| `state.hover`, `state.pressed` | `fg.default` at 8% / 16% alpha composite (theme-agnostic) |
| `focus.ring` | `accent` (high-contrast variant: `text`) |
| `divider`, `outline.strong` | `border`, `border` darkened/lightened toward `text` by fixed 10% |
| `status.success/warning/info` | fixed accessible pairs per variant (light vs dark) — only non-derived constants allowed |
| `danger.default/fg` | `danger`, computed contrast-safe on-text |
| `radius.s/m/l` | `cornerRadius`-anchored: `cornerRadius/2`, `cornerRadius`, `cornerRadius*1.5` (≤32 clamp) |
| `space.1..6` | fixed 4-px grid: 2, 4, 8, 12, 16, 24 (density-independent logical px) |
| `type.scale` | 5 steps anchored on schema `fonts.pointSize`: caption ×0.85, body ×1.0, subtitle ×1.25, title ×1.5, display ×2.0 |
| `motion.instant/short/base/long` | 0, `max(80, motionDuration×0.6)`, `motionDuration`, `motionDuration×1.75` |
| `elevation.1..3` | blur/shadow params derived from `blurEnabled` + variant |

Accessibility transforms are token-level, not per-control: `highContrast`
swaps the active theme to `qinda-high-contrast`; `reducedMotion` clamps every
`motion.*` to 0/80 ms; `reducedTransparency` forces alpha-derived tokens to
opaque equivalents; `accessibility.textScale` multiplies `type.scale`.
This is implementable immediately against existing schema keys
(`data/settings/schema-v1.json`, accessibility domain) **[E]**.

Public surface: one frozen C++ value struct + one QML singleton
`QindaQt.Tokens 1.0` (§4). Derivation rules live in exactly one C++ file with
property tests; QML only reads. Expansion of Tier 2 requires a revision bump
of this table, not a schema change, as long as it stays derivable; anything
needing new theme data goes through theme schema v2 + migration [P].

## 4. Reusable process/module boundaries and contracts [P]

All new modules follow the existing source-ownership table format
(`docs/wiki/architecture/module-boundaries.md`). Proposed additions:

| New module | Owns | Allowed inward dependencies |
| --- | --- | --- |
| `src/design_tokens` | QST-1 derivation from `ThemeSpec` + accessibility settings values; frozen `DesignTokens` value; QML singleton registration; contrast validation helpers | Public `themes` values; Qt Core/Gui; a *public* settings value snapshot supplied by the caller (never the settings service directly) |
| `src/controls` | `QindaQt.Controls 1.0` QML module: token-styled Qt Quick Controls 2 style + ~12 QindaQt-specific controls (SectionHeader, StateCard, ThemeCard, DegradedNotice, FocusRing, ColorToken swatch, FormRow, …) | `design_tokens` QML singleton; Qt Quick/QuickControls2/Layouts |
| `src/appshell` | `QindaQt.AppShell 1.0`: ApplicationWindow scaffolding, sidebar/rail + StackView navigation, route registry API, window-state persistence hook, RTL/keyboard chrome | `controls`, `design_tokens`; Qt Quick Controls 2 |
| `src/apps/settings_center` | The `qindaqt-settings` executable, per-domain view models, route registrations | Public SDK: settings client, `profiles`, `themes`, `appshell`, `controls`; never shell internals |
| `tests/{design_tokens,controls,appshell}` | Focused offscreen tests + visual-baseline fixtures | Public APIs only |

### Explicit contracts (each new public interface must state these)

- **DesignTokens (C++ value + QML singleton).**
  *Ownership:* produced by `src/design_tokens`; consumed read-only everywhere.
  *Lifetime:* QML singleton lives with the engine; C++ `DesignTokens` is an
  immutable value — a theme or accessibility change produces a **new** value,
  never mutation. *Threading:* derivation is a pure function callable on any
  thread; the QML singleton is GUI-thread only. *Error:* unknown theme ID →
  resolved from `qinda-dark` + a returned diagnostic; invalid settings values
  are unreachable because inputs are validated settings values; derivation is
  total (no partial tokens). *Accessibility:* token values ship with
  computed contrast ratios; a build-time test fails if any built-in theme
  violates WCAG 2.2 AA for its own fg/bg pairings (§10).
- **QindaQt.Controls.** QML-only public API. *Contract:* no control reads
  anything except `QindaQt.Tokens` and its own properties; no control imports
  `QindaQt.Shell` or any service module. Breaking visual API requires a minor
  version bump of the module.
- **Route registry (in `src/appshell`).**
  `SettingsRoute { id, title, icon, keywords, availability, viewModelFactory,
  pageComponent }`. *Contract:* the registry is an ordered insert-only
  structure owned by the app executable; domains register at startup; search
  and keyboard navigation consume the same registry. `availability` is a
  declarative probe result (`Available / RequiresService("…")`), so future
  platform services light routes up without page edits. *Error:* duplicate
  route ID aborts registration loudly (programmer error).
- **Settings view models.** One small view model per domain
  (e.g. `AppearanceViewModel`), each owning: typed settings keys, a
  `SettingsState` projection (Loading/Ready/Saving/Conflict/Unavailable —
  matching the manager's accepted UI boundary decision in
  `persistent-notification-quieting/1787796417-manager-boundary-decision.md`),
  transaction submission through the public Settings1 client only, and
  conflict-retry semantics. *Contract:* QML never touches the settings client
  directly; view models never import QML or shell types; a lost service holds
  the last confirmed value and forbids new writes until reauthenticated
  (same failure policy the manager already accepted for DND).
- **App shell window.** Ordinary Wayland top-level; owns no desktop-global
  authority; no layer surfaces; single instance per session is a launch
  convention (desktop entry), not an enforced singleton.
- **Testing boundary.** All modules offscreen-testable with the software
  renderer (precedent: `qindaqt.notification-surfaces-offscreen`); visual
  baselines follow the testing harness (pinned fonts/locale/time/animation
  clock, perceptual compare, human review for intentional changes)
  **[E — harness rules; M — no app baselines exist yet]**.

## 5. Settings Center information architecture [P]

Window: `qindaqt-settings`, `QindaQt.AppShell` sidebar layout. Sidebar =
route registry order; global search filters registry keywords. The
`notifications` route and its launch integration already have an accepted
contract (manager decision, see §4) and are carved out of this proposal.

Route table — availability is declared against **today's** reality:

| Route | Settings keys (schema v1/v2) | Backing at slice time |
| --- | --- | --- |
| Appearance | `appearance.theme`, `accentColor`, `blurEnabled`, `animationsEnabled`, `animationDurationMs` | Fully live: ThemeCatalog + token layer **[E]** |
| Wallpaper | `appearance.wallpaper` | Stored value only; shell has no wallpaper renderer yet → DegradedNotice "stored; applies when desktop wallpaper lands" **[M]** |
| Fonts | `fonts.*` (family, mono, size, antialiasing, hinting, subpixelOrder) | Live for QindaQt apps via tokens; global fontconfig application is a Platform-service slice → honest split stated on-page **[P/M]** |
| Displays | `displays.*` | No output-configuration service at base → route present, all controls DegradedNotice'd "Requires the display service (Platform services milestone)" **[M]** |
| Input | `input.*` | Same: requires input/platform adapter **[M]** |
| Panels & Layout | `panels.layoutProfile`, `panels.autoHideDelayMs` | Live: ten built-in profiles, preview screenshots, editor launcher (§6); autoHide delay is stored until visibility transport consumes it **[P/M]** |
| Window Management | `windowManagement.*` | Stored; compositor consumption needs a Settings1→compositor adapter (Platform/compositor lane) → degraded **[M]** |
| Notifications | `services.doNotDisturb` (schema v2 of the Settings1 candidate) | Live after Settings1 integration; this route is Ada's accepted scope — I claim only the shell chrome around it **[E/P]** |
| Accessibility | `accessibility.*` | Live via token transforms (§3) for highContrast/reducedMotion/reducedTransparency/textScale; `screenReader` toggle stored until AT-integration qualification **[P/M]** |
| Services & Session | `services.*` | Each key degraded with the owning service named (clipboard, metrics, bluetooth, portals) **[M]** |

Rules: (a) every control shows its effective value's source layer on hover /
Accessible.description ("User setting" / "Profile default" / "System
default") — the settings model already reports source layers **[E]**;
(b) Saving/Conflict/Unavailable states are uniform `StateCard` components;
(c) a conflict refreshes and requires explicit retry — never silent
overwrite; (d) no route links to a service that cannot answer, and no route
claims a stored value is applied when no consumer exists.

## 6. WYSIWYG drag palette/editor interaction contract [P]

The pure transaction engine already exists — the editor is *only* an adapter
over it; this is what keeps it cleanly separated from shell policy.

- **Palette.** Three sources, rendered identically: applets (from the
  validated manifest catalog — presentation names only; "the editor never
  treats the manifest payload in a drag source as authority" is existing
  policy **[E]**), panel fragments (top/bottom/left/right × alignment), and
  whole profiles (ten built-ins + user profiles).
- **Drop targets.** Highlighted edges/zones come exclusively from
  `LayoutEditingCoordinator::evaluate()` at the current revision — no
  duplicated placement logic in QML; acceptance reserves nothing and is
  discarded on any revision change (existing semantics **[E]**). The canvas
  renders the repository's solved-layout snapshot, not shell surfaces.
- **Command mapping.** Drag-drop, palette activation, and keyboard emit the
  same typed `EditingCommand`s. Keyboard-first contract: palette items are
  focusable; Enter inserts into the selected anchor; arrow keys move the
  selection between panels/zones/instances; Ctrl+arrows reorder; Delete
  removes; Ctrl+D duplicates; Ctrl+Z / Ctrl+Shift+Z (and Ctrl+Y) undo/redo;
  F5 begins live preview, F6 commit, F7 cancel. A settings page lists every
  binding (existing constraint: keyboard equivalents are required
  **[E]**).
- **Undo/redo & preview.** Repository-owned durable history; preview edits
  are provisional and cancel restores the exact pre-preview profile in one
  revision; commit collapses the preview into one durable undo step (existing
  **[E]**). The editor adds **no** persistence of its own.
- **Live preview plumbing [M].** The production surface host does not yet
  subscribe to provisional editor snapshots (stated limitation in
  `docs/wiki/shell/layout-profiles.md`). Closing this is *shell-lane* work:
  `shell_orchestration` consumes editor snapshots the same way it consumes
  committed plans. The editor window itself never touches LayerShellQt.
- **Failure recovery.** Any rejected command leaves snapshot/revision/history
  unchanged by construction; the editor surfaces a typed toast
  (e.g. `ManifestUnavailable`, `UnsupportedAppletPlacement`, stale revision →
  auto-refresh targets). Loss of the coordinator lease → read-only banner +
  re-acquire on next user action, never a second editor. Editor crash →
  committed profile is untouched and previews vanish with the process (by
  design). Output inventory change mid-session cancels the editor session
  (existing repository rule **[E]**).
- **Persistence [P, needs ADR].** User edits save as *derived user profiles*
  (existing product rule **[E]**); selection persists through
  `panels.layoutProfile` via Settings1. Exact file ownership/versioning of
  user profiles needs one ADR in that slice (collision note: profiles module
  owns the format; settings service owns persistence once it exists — do not
  invent a second JSON writer inside the app).

## 7. Theme mapping: Qinda macOS and the built-ins onto shared tokens [P]

All first-party UI resolves the same QST-1 derivation; "Qinda macOS style"
for applications is *not* fake window chrome — traffic lights and tab
direction are decoration-shell concerns consumed only by the KDecoration
plugin and shell chrome (`DecorationButtons.qml`, `ContainerTabStrip.qml`
already do this **[E]**). Applications consume:

| Tier-1 token | Qinda macOS | Qinda Light | Effect in any app |
| --- | --- | --- | --- |
| `canvas/surface/surfaceRaised` | `#9FB8B2/#E7EFEC/#F7FAF9` | `#e9e8e4/#f7f6f2/#ffffff` | window/page/card fills |
| `text/textMuted` | `#17231F/#60716C` | `#20211f/#666963` | typography |
| `accent/accentText` | `#4DAF98/#0A2921` | `#386a64/#ffffff` | controls, selection, focus |
| `danger` | `#FF5F57` (also closeColor) | `#b23a3a` | destructive actions |
| `cornerRadius`/`motionDuration`/`blurEnabled` | 12 / 180 / true | 10 / 150 / false | radius ramp, motion ramp, elevation style |

The five built-ins therefore differ only by data; zero per-app conditionals
are permitted (enforced by a source-shape-style grep test: app QML must not
contain theme IDs or hex literals outside the token module [P]).
High-contrast is both a theme and an accessibility switch; they resolve to
the same token transform (§3).

## 8. Responsive layouts and QA matrix [P]

Layout system: Qt Quick Layouts everywhere; no absolute positioning except
canvas previews; breakpoints are tokens (`AppShell.breakpoint.rail < 900
logical px width → icon rail; < 640 → single-column forms; minimum window
560×420 logical`). The notification center's proven compact behavior (usable
at 384 px; degraded gracefully at 400×300 **[E]**) is the precedent for
degraded states, and `shell_layout`/production-surface tests already cover
the panel-side matrix at 1080p/WUXGA/1440p **[E]**.

Exact QA rows for first-party app windows (each row: sidebar form, expected
adaptation, gate):

| Row | Width×height / scale | Expected adaptation | Gate |
| --- | --- | --- | --- |
| A1 | 1920×1080 @100% | Rail + two-pane (nav + form) | offscreen size-invariant test + preview screenshot |
| A2 | 1920×1200 WUXGA @100% | Same as A1; extra vertical form breathing | same |
| A3 | 2560×1440 @100% | Same; content column max-width 720 px, centered | same |
| A4 | 1920×1080 @125% | Logical layout identical to A1; no px-hinting artifacts in text/screenshots | mixed-DPI screenshot pair |
| A5 | 1920×1080 @150% | Same as A4; icon set swaps to @2x assets where provided | same |
| A6 | 2560×1440 @125% and @150% | As A3 | same |
| A7 | Narrow/windowed 720×480 | Sidebar → icon rail, single column, action overflow menu | offscreen 720×480 test |
| A8 | Compact 560×420 (minimum) | Rail → hamburger; all primary actions reachable | offscreen + manual matrix |
| A9 | 400×300 stress | Page still renders honest degraded/scrollable state (notification-center precedent) | offscreen, no-crash assertion |

Panels/desktop rows reuse the existing required display matrix verbatim
(1080p @100/125/150%, WUXGA + portrait, 1440p @100/125/150%, multi-output and
mixed-DPI scenarios — `docs/wiki/development/testing-harness.md` **[E]**).
M — no app-window rows exist yet; all nine are new gates for the controls/
appshell slices.

## 9. Performance, memory, startup budgets and measurement [P]

Product floor (existing): shell+compositor+resident services ≤ 500 MiB idle
PSS, < 1% idle CPU **[E]**. Lane budgets (new, to be ratified in the
design-tokens ADR):

- `qindaqt-settings` cold start → first interactive frame ≤ 400 ms on the
  reference machine; route switch ≤ 100 ms; theme switch re-render within
  one frame (≤ 16.7 ms) — only the owning pages repaint, driven by the
  settings `SettingsChangeSet` effective-change records **[E — mechanism]**.
- Design-token derivation (all roles, all five themes) ≤ 1 ms per resolve —
  unit benchmark with `QElapsedTimer`, asserted in CI.
- `QindaQt.Controls` + tokens resident cost ≤ 15 MiB PSS over a bare
  Qt Quick window — measured by `/proc/<pid>/smaps_rollup` Pss sampling in a
  repeatable tools/ script (ignored build output only).
- QML compilation: all new modules compile via `qt_add_qml_module` with qmlcachegen; lint clean (`all_qmllint`) per existing gate **[E — tooling]**.
- Measurement policy: budgets are CTest benchmarks (fail on regression) plus
  a manual matrix script; numbers are recorded per candidate handoff like
  test counts. Startup measured with `QT_LOGGING_RULES qml*.debug=false` +
  external `time`, median of 10, offscreen platform.

## 10. Accessibility, keyboard, localization, RTL, fonts, contrast, motion, screen readers [P unless noted]

- **Contrast:** WCAG 2.2 AA minimum (4.5:1 normal text, 3:1 large text and
  UI components); `qinda-high-contrast` targets ≥ 7:1 body text. Enforced
  computationally in the token module tests (§4). Standard:
  https://www.w3.org/TR/WCAG22/
- **Keyboard:** full tab chain, visible 2-px `focus.ring` (never
  `focus: false` styling removed), arrow-key navigation within lists/rails,
  Escape closes dialogs/popups (precedent: notification center
  window-scoped Escape **[E]**), no pointer-only operation anywhere.
- **Screen readers:** every control carries `Accessible.role`, `name`,
  `description`; state changes (Saving/Conflict/Unavailable, theme applied)
  raise accessible events so AT announces them; images/decoration have
  `Accessible.ignored: true`. Offscreen tests assert the accessible tree
  shape (precedent: notification offscreen suite **[E]**). Live AT-bridge
  qualification remains a display-matrix gate — explicitly out of offscreen
  scope, as the harness already distinguishes **[E]**.
- **Localization:** all strings via `qsTr()`/`QT_TR_NOOP`; `qt_add_transitions`
  no — `qt_add_translations` (lupdate/lrelease) wired per module; no
  concatenated sentences; layouts must absorb +40% string growth without
  truncation (QA row addition per release).
- **RTL:** `LayoutMirroring.childrenInherit: true` at window root; all
  geometry via Layouts/anchors-aware APIs; icons with directional semantics
  flip; locales forced to an RTL language in the offscreen matrix. Note the
  deliberate distinction: Qinda macOS's right-to-left *container tabs* are a
  fixed decoration design choice, not locale mirroring **[E vs P]**.
- **Font smoothing:** schema keys `fonts.antialiasing/hinting/subpixelOrder`
  exist **[E]**; QindaQt apps consume family/size through tokens;
  antialiasing/hinting/subpixel apply through platform font configuration in
  a Platform-services slice — until then the Fonts route states exactly that
  (no fake toggle).
- **Reduced motion/transparency:** token-level clamps (§3); animations gate
  on `motion.*` tokens only; a token test asserts zero >80 ms animations
  under reducedMotion.

## 11. Reuse decisions: Qt, Kirigami, KDE [P]

Primary references: Qt Quick Controls
(https://doc.qt.io/qt-6/qtquickcontrols2-index.html), styles
(https://doc.qt.io/qt-6/qtquickcontrols-styles.html), customization
(https://doc.qt.io/qt-6/qtquickcontrols-customize.html), layouts
(https://doc.qt.io/qt-6/qtquicklayouts-index.html), scalability/DPI
(https://doc.qt.io/qt-6/scalability.html), performance
(https://doc.qt.io/qt-6/qtquick-performance.html), accessibility
(https://doc.qt.io/qt-6/accessible.html), i18n
(https://doc.qt.io/qt-6/internationalization.html), RTL
(https://doc.qt.io/qt-6/qtquick-positioning-righttoleft.html), Kirigami
(https://develop.kde.org/docs/getting-started/kirigami/). All links verified
live on 2026-08-27.

- **Own outright (QindaQt identity and boundaries):** the token system, the
  Controls style, navigation, settings IA, editor UX. Qt Quick Controls 2 is
  the base control set — we implement a QindaQt *style* (styles are Qt's
  supported customization mechanism) plus a small custom-control layer, not
  from-scratch controls.
- **Reuse as-is:** Qt Quick/Layouts/QuickControls2 (compiled QML),
  LayerShellQt (already behind `shell_surface` adapters), KDecoration3,
  KF6 GlobalAccel (ADR-0009 **[E]**), QSaveFile-backed settings persistence
  (existing **[E]**).
- **Kirigami 6.27: selective, adapter-only — do not adopt as the app
  framework for v1.** Rationale: (a) Kirigami theming derives from the KDE
  color-scheme/Units stack, which would make a second token authority compete
  with QST-1 and re-couple first-party visuals to KDE HIG; (b) its
  settings conveniences (CategorizedSettings/FormCard pages) assume Kirigami
  page/action models that don't match the route-registry + view-model
  contract above; (c) license (LGPL-2.1+) and availability are fine, so the
  door stays open. Accepted reuse: nothing mandatory today; if a future
  slice wants Kirigami components (e.g. InlineMessages), it goes through a
  thin `src/controls` adapter wrapper so the dependency stays swappable and
  invisible to app code. This is a durable cross-cutting choice → one ADR in
  slice 1.
- **KGuiAddons color utilities:** permitted privately inside
  `src/design_tokens` for contrast/blend math (already installed, narrow,
  no KConfig coupling); must not leak into public token types.

## 12. Dependency graph and vertical slices [P]

```
 themes(E) ─► design_tokens(S1) ─► controls(S2) ─► appshell(S3) ─┬► settings_center pages(S4)
 settings1(E, in review) ───────────────────────────────────────┘            │
 profiles/shell_customization(E) ─► editor window(S5) ◄─ shell snapshot subscription (shell lane)
 appshell(S3) ─► availability/registry plumbing(S6, parallel)
```

| Slice | Outcome | Path ownership (new) | Test gates | Docs/ADR |
| --- | --- | --- | --- | --- |
| **S1 tokens** | QST-1 derivation, QML singleton, contrast tests | `src/design_tokens/**`, `tests/design_tokens/**`, additive line in `src/CMakeLists.txt` | unit+property derivation tests, benchmark, 5-theme contrast gate, offscreen singleton test | new wiki `architecture/design-tokens.md`, ADR "own QST-1, not Kirigami theming"; module-boundaries row |
| **S2 controls** | `QindaQt.Controls 1.0` core set + style | `src/controls/**`, `tests/controls/**` | offscreen per-control tests incl. accessible tree, first visual baselines (5 themes × 3 sizes) | wiki `shell/controls.md`; baseline fixtures |
| **S3 appshell** | Window scaffolding, route registry, rail/sidebar breakpoints | `src/appshell/**`, `tests/appshell/**` | navigation/RTL/keyboard offscreen tests; rows A1–A8 offscreen | wiki `apps/app-shell.md`; mkdocs.yml link |
| **S4 settings pages** | Appearance + Accessibility routes live on real settings client; degraded IA for the rest | `src/apps/settings_center/**` (post-integration; Ada's candidate introduces the app), `tests/apps/settings_center/**` | view-model transaction/conflict tests; route availability tests; screenshot baselines | settings-service wiki IA section; TASK_LIST update at integration |
| **S5 editor window** | Drag palette + canvas over `LayoutEditingRepository` | `src/apps/settings_center/editor/**` or `src/apps/customize/**`, focused tests | command-parity tests (drag ≡ keyboard ≡ command), lease-loss, preview commit/cancel UI; rows A1–A8 | extend `shell/layout-profiles.md` edit section |
| **S6 availability plumbing** | Route registry probes + DegradedNotice system-wide | `src/appshell` + `src/controls` additive | registry probe tests | coding-practices "honest states" note |

Coordination notes vs. current work: (1) Ada Ruiz owns
`src/apps/settings_center`, `src/services/settings_*`, `src/settings`,
settings wiki pages, and ADR-0012 until integration — S4/S5 must not exist in
parallel worktrees before her candidate lands; S1–S3 don't collide with any
owned path. (2) The live-preview snapshot subscription (S5 dependency) is
shell/customization-lane work; coordinate, don't absorb. (3) Shared
registries (`src/CMakeLists.txt`, tests registry, `mkdocs.yml`, source-shape
config) are additive-only edits with board notice. (4) Rowan Ivers / Talia
North / Iris Vale are engaged reviewers on the Settings1 thread; a
different-worker review applies to each slice candidate per team policy.

## 13. Recommended first code slice [P]

**Start with S1 (design tokens) immediately after the Settings1 integration
lands.** Justification: S1 compiles against only `src/themes` + Qt, all of
which exist and are frozen at base **[E]**; it has zero overlap with
Settings1 paths; every later slice depends on it; and its ADR settles the
Kirigami question before UI code exists. If the manager wants a slice that
can start *before* integration without any shared-file risk, S1 is also safe
then — but only in its own isolated worktree, per team rules. S2 follows
once S1's baselines are approved; S4 starts only after integration, reusing
the accepted `notifications` route pattern as its structural template.

## 14. Explicit non-goals

- No Plasma, no KCM/Systemsettings framework, no plasma-* components.
- No Kirigami adoption in first-party v1 (adapter-only if ever).
- No theme schema v2 in these slices (derivation stays schema-compatible).
- No per-app hard-coded styling or per-app theme overrides.
- No settings applet, no global shortcut, no layer-shell settings window
  (excluded by the accepted manager UI boundary **[E]**).
- No wallpaper rendering, no live display/input/audio adapters, no AT-bridge
  live qualification — those belong to Platform services / display-matrix
  gates.
- No third-party applet UI or sandboxed-host surfaces in the editor palette.
- No QWidget-based settings UI; Qt Widgets remain allowed elsewhere per
  design rules but not for this app.

## 15. Coverage honesty

Everything in §1 is reproducible at `dc29c8891`. Sections 2–14 are
proposals; none of `src/design_tokens`, `src/controls`, `src/appshell`, the
QA rows, budgets, or baselines exist today, and no claim here should be read
as implemented. No build, test, or live-desktop command was required or run
beyond read-only inspection (`git status`, `ls`, file reads, `pacman -Q`,
HTTP HEAD/GET on the cited documentation URLs). No live desktop, session
bus, input device, cursor, or global shortcut was touched.

— Juno Park, 2026-08-27T17:58:35Z
