# QindaQt Settings — Appearance route

`qindaqt-settings --page appearance` is the first-party Appearance settings
surface. It is one modular route beside the Notifications route inside the
ordinary `qindaqt-settings` Qt Quick application. The domain module
`src/apps/settings/appearance` owns validated appearance values, the QST-1
preview projection, the Settings1-backed route model, and the narrow installed
KWin-decoration controller; the executable owns only their additive route
seams. The durable composition decisions are recorded in
[ADR-0028](../adr/0028-compose-appearance-settings-through-settings1.md) and
[ADR-0160](../adr/0160-select-installed-kwin-window-decorations.md).
The route keeps its transaction state machine separate from preview projection
and divides presentation into dedicated Themes, Wallpaper, and Fonts
destinations. The destinations share one route-level draft and Apply/Revert
boundary; none imports persistence or platform APIs.

## What the route offers

One page covers the appearance preference set stored through Settings1:

| Group | Controls | Settings1 keys |
| --- | --- | --- |
| Themes | A preview window (ADR-0127) painting the previewed theme's real window chrome through the decoration painter the compositor uses, around the real Fusion controls ordinary Qt applications get, over the draft wallpaper; sixteen built-in theme cards, each a rendered thumbnail of that theme's chrome with its paired decoration document and its panel material (ADR-0206); the system/light/dark scheme choice; the **Translucency** and **Motion** switches; and the palette row naming each QPalette role on hover | `appearance.theme`, `appearance.colorScheme`, `accessibility.reducedTransparency`, `accessibility.reducedMotion` |
| Windows | A separate catalog of installed native and Aurorae KWin decorations with an explicit **Use decoration** action; a **Window decoration** chooser of decoration documents (ADR-0207) painted by the decoration painter; QindaQt-only application-window controls and shared-painter preview while QindaQt is selected; a **Container decoration** chooser painted by the compositor's container renderer; and an independently truthful two-window container preview with button and tab controls (ADR-0129, ADR-0160) | KWin `[org.kde.kdecoration2]` `library`/`theme`; Settings1 `appearance.windowDecoration`, `appearance.containerDecoration`, `appearance.windowButtonStyle`, `appearance.windowButtonSide`, `appearance.windowButtons`, `appearance.windowTitleAlignment`, `appearance.containerButtonStyle`, `appearance.containerButtonSide`, `appearance.containerTabOrder`, `appearance.containerButtonGlyphs`, and the ADR-0264 title-bar options `appearance.windowButtonSize`, `appearance.windowButtonSpacing`, `appearance.windowTitleHeight`, `appearance.windowCornerRadius`, `appearance.windowTitleWeight`, `appearance.windowAppIcon`, `appearance.windowRollUpButton`, `appearance.windowTitleDoubleClick`, `appearance.containerButtonSize`, `appearance.containerButtonSpacing`, `appearance.containerTitleDoubleClick` |
| Wallpaper | Bundled previews (any of png/jpg/jpeg/webp/bmp beneath the wallpaper data directories, ADR-0228), native image chooser or local path, and scaled/centered/tiled mode | `appearance.wallpaper`, `appearance.wallpaperMode` |
| Fonts | Independent interface and installed fixed-width family pickers with live samples and saved/draft monospace state, size slider (6–36 pt), antialiasing, hinting, and subpixel choices | `fonts.family`, `fonts.monospaceFamily`, `fonts.pointSize`, `fonts.antialiasing`, `fonts.hinting`, `fonts.subpixelOrder` |

Display scale belongs to the separate **Display** route, which owns the live
output configuration. Appearance offers a direct route action rather than a
second, stored-only scale control.

The Wallpaper destination's preview and the path field read the route
model's projections, never their own resolution: the preview paints the same
`previewWallpaper` file URL the Themes and Windows destinations use, and the
path field adopts draft changes that come from the bundled grid, the "No
wallpaper" choice, the file dialog, Revert, or a baseline rebase while
leaving the user's own in-progress typing untouched.

The Themes preview and palette row are presentation-only. Its chrome is
`DecorationChrome::fromTheme` (the derivation the QindaQt compositor publishes)
painted by the shared decoration painter, and its client area is the Fusion
`QStyle` painted with `nativeAppearance`'s palette and the draft font. It
therefore previews the QindaQt appearance theme rather than claiming to render
an unrelated active KDecoration plugin. The preview follows the draft: a theme
card, scheme, or font change shows before Apply. Without a widgets application
(headless tests) the client area degrades to flat palette rows.

The Windows destination reads the real KWin decoration selection separately.
It discovers valid Aurorae theme directories from the standard XDG data roots
and supported native plugins from Qt's plugin roots. Applying one choice
preserves unrelated `kwinrc` keys and synchronously requests KWin reconfigure.
QindaQt's renderer is shown only for QindaQt; a foreign plugin's authoritative
preview is the Settings window's own frame after apply. Container chrome is
always available because the QindaQt compositor owns that renderer regardless
of the application-window decoration plugin.

Every built-in theme authors real decoration behavior. The current catalog
contains at least four distinct families across left/right controls,
traffic-light/symbol/glyph styles, tab direction, hover glyphs, and colors.
The theme-card flow reports its wrapped height to the scroll layout, so later
cards are visible rather than existing only in the model or preview. External
legacy themes may still use the schema's unauthored compatibility fallback.
See [ADR-0159](../adr/0159-author-distinct-decoration-presets-for-every-builtin-theme.md).
Since [ADR-0207](../adr/0207-decoration-themes-are-their-own-documents.md)
a theme may instead name a decoration document from `data/decorations`, and
the two chooser keys select `theme` (follow the color theme's pairing) or any
installed document id for windows and for containers separately; the
arrangement rows still refine whichever document is in effect. The chooser
cards, the theme thumbnails and both previews are painted by the same
renderers from the draft theme, document, arrangement and wallpaper, so the
route never shows a mock of a material.

Since [ADR-0264](../adr/0264-window-button-styles-are-data.md) the **Buttons**
row of both sets is a menu of the same fifteen named styles (Lights, Flat,
Glyphs, Gel, Bevel, Blue tiles, Wide, Tab, Bold, Minimal, Pills, Dots, Outline,
Chunky, or the theme's own). Each is a row of data for the one style painter,
so the window preview, the container preview and the desktop draw the same
buttons. Under the arrangement rows, **Button size**, **Button spacing** and
**Double-click** appear for both sets, and windows add **Title bar** height,
**Corners**, **Title weight**, **App icon** and a **Roll-up button**. The
window roll-up button and a roll-up double-click roll the window up to its
icon ([ADR-0203](../adr/0203-an-ordinary-window-rolls-up-to-its-icon.md));
maximize and minimize are the window manager's own actions. The window
double-click's **Default** is the theme's double-click where the theme
authors one (Qinda Marigold rolls up), else KWin's own
([ADR-0268](../adr/0268-familiar-desktop-experiences-are-layout-and-theme-pairs.md)),
and the container's **Nothing** keeps its bar inert, as both shipped. A theme
may also put the roll-up control in minimize's place (Qinda Classic Grey)
and paint its colored title bar clean. A container
bar's height belongs to the container layout, so containers have no height
option. The rows live in `TitleBarOptions.qml`, built once per set.

The page is QST/Controls-only: QindaQt.Controls primitives, QST-1 semantic
roles, `Accessible` names/descriptions/roles on every control, radio
semantics for the scheme and enum choices, an explicit initial focus on the
first theme card, and a visible focus chain through the draft action row. A
single `QindaQt.Controls` tab strip above the form selects Themes, Windows,
Wallpaper, or Fonts at every width — glyph-first tabs on one shared rule with an accent
indicator, each explained by a tooltip and accessible description rather
than a paragraph — so Appearance does not introduce a second vertical
navigator beside the Settings Center's route sidebar. The form has a visible vertical
scrollbar, Page Up/Page Down and Ctrl+Home/Ctrl+End scrolling, and automatic
focus reveal; every forward and reverse Tab stop stays inside the compact
420×320 viewport.

## Truthful state surface

The route model projects the same Loading/Ready/Saving/Conflict/Unavailable
truth as the DND controller, extended to a draft workflow:

- **Draft** — edits accumulate locally and are validated immediately
  (installed theme, non-empty family, installed fixed-width monospace family,
  6–36 pt, 0.5–3.0 scale, no embedded NUL). Invalid fields expose per-key
  error text and disable Apply; they never reach Settings1. The monospace
  picker lists Qt's installed fixed-width families and keeps an in-progress
  typed name in the shared draft before focus moves.
- **Preview** — the draft drives one complete published QST generation, so
  the page chrome, theme previews, and font sample stay consistent. When the
  configured theme id is not installed, the page shows which theme the
  scheme preference would select instead; it never silently renames the
  stored preference.
- **Apply** — writes only the changed keys, one public single-key optimistic
  commit at a time in fixed key order. The model waits for each fresh
  authoritative snapshot before the next key, so no write uses a stale base
  revision. Per-key outcomes are reported truthfully; the sequence is never
  claimed to be atomic. A bounded accessible result ledger names every key in
  the captured sequence as Applied, Failed, Conflict, Uncertain, or Not
  attempted, so a later-key failure cannot hide an earlier durable success.
- **Revert** — discards the draft and republishes the confirmed generation.
  Revert is refused while a commit sequence is in flight. From an answerable
  Conflict it also clears conflict intent, confirmed diagnostics, and the
  previous result ledger before returning to clean Ready.
- **Conflict** — a rejected key whose authority differs stops the sequence,
  keeps the draft, and requires an explicit re-Apply (or Revert) against the
  refreshed baseline. The controls remain non-editable during the commit-
  reply-to-snapshot gap; after the same owner/epoch supplies a fresh snapshot,
  the retained draft becomes editable again. If authority already equals the
  draft, the key counts as done.
- **Uncertain/loss** — timeouts, owner replacement, and bus loss surface the
  last confirmed values, forbid writes, and never replay anything; Retry
  refreshes authority only, and a new explicit Apply is the only resubmit.
  An owner/epoch replacement between a successful reply and its authoritative
  snapshot also discards every queued key while retaining the draft.
- **Confirmed failures** — validation/persistence/rejection diagnostics stay
  visible across automatic rebaselines until a new explicit write dismisses
  them.

Draft intent is tracked per key rather than inferred from one stale draft
snapshot. Every untouched field rebases to a later same-owner or replacement-
owner snapshot; only fields the user actually edited survive. If authority
changes Wallpaper while Theme alone is edited, Apply sends Theme only and
never restores the stale Wallpaper.

## Settings Center composition seam

The executable `qindaqt-settings` composes the two current routes once per
process so [Settings Center navigation](settings-center.md) can switch pages
without discarding confirmed state:

1. parse `--page` (`notifications` unchanged, `appearance` additive,
   otherwise exit 2 with the existing diagnostic);
2. scope one public `SettingsClient` to `AppearanceKeys::scopedKeys()` for
   the appearance model and a separate client to notification quieting; each
   client owns an independent `QtSettingsTransport` so their local request
   tokens cannot collide on one signal source;
3. discover bundled wallpapers (png/jpg/jpeg/webp/bmp, ADR-0228) from `$XDG_DATA_DIRS/qindaqt/wallpapers` and the installed prefix, with earlier roots winning duplicate identities and the priority format winning a duplicate basename inside one root; choosing one writes its portable `qindaqt:<basename>` identity while a saved custom path or empty choice remains unchanged until the user edits and applies it;
4. merge every theme directory from the same search contract as the text
   editor (`$XDG_DATA_DIRS/qindaqt/themes`, then beside the installed
   executable; `--theme-directory` prepends a developer path). Earlier
   directories win duplicate IDs, while unique built-ins remain present; an
   invalid theme fails closed and no themes exits 3 instead of rendering
   token-less controls;
5. construct the KWin-decoration controller with the user's `kwinrc`, standard
   Aurorae data roots, and the bounded KWin reconfigure call; it performs no
   write until the user explicitly invokes **Use decoration**;
6. bind the engine-owned `QindaQt.Tokens` singleton, hand it to the model,
   and only then load `Main.qml`; one presentation-active route host
   instantiates exactly one route component with its required model property,
   while both bounded domain models remain alive. The executable adds
   the generated build QML root only when it is actually running from that
   build tree; installed/relocated runs use the prefix's `lib/qt6/qml` root
   and a relative Tokens RUNPATH, never developer import paths.

QML never consumes either settings client or transport directly. The
navigation library owns no appearance values or notification policy, and each
page receives only its own model even though both models share the process.

## Deliberate non-goals for this slice

- Settings1 Apply never mutates compositor or shell surfaces directly. The
  separate **Use decoration** action changes only KWin's documented decoration
  configuration and requests one live reload. After Settings1 Apply publishes
  a confirmed snapshot, the production shell adopts appearance preferences.
- Font families are listed from the local Qt font database for selection. The
  monospace catalog filters to fixed-width families and checks typed draft
  names against that catalog. Where Qt reports an installed family as variable
  pitch, the catalog accepts it only if its styles resolve to that family and
  representative glyph advances are equal. This admits the shipped Noto Sans
  Mono default on Qt 6.11 without offering proportional Noto Sans or a missing
  saved family as a new choice. The persisted values remain separate validated
  strings: `fonts.family` changes interface text; `fonts.monospaceFamily`
  flows through FontSettingsBridge and the Qt platform theme's `FixedFont`.
  First-party session bootstrap applies the confirmed preferences before
  application construction. A missing previously saved family stays visible
  as confirmed state so the user can choose an installed replacement.
- No multi-key atomic transactions: the public client exposes single-key
  writes only; see ADR-0028 for the batch follow-up boundary.
- No accessibility-domain coupling: text scale, reduced motion, and reduced
  transparency stay in their own route; the preview derives high contrast
  only from the dedicated theme variant.

## Verification

Focused selectors:

```sh
ctest --test-dir build/dev \
  -R '^qindaqt\.appearance-' --output-on-failure
```

- `qindaqt.appearance-values` — bundled-wallpaper discovery precedence, token round trips, canonical decode, typed
  rejections including empty non-empty-schema strings, draft validation, and
  exact shipped-schema key/default/constraint contracts.
- `qindaqt.appearance-preview` — configured-theme precedence, scheme and
  platform fallbacks, complete preview maps for every built-in theme,
  high-contrast QST input, and an exact six-ID inventory including
  `qinda-macos` and `qinda-bliss`.
- `qindaqt.appearance-settings-model` — baseline decode, per-key commit
  sequence with fresh-base snapshots, conflict stop/explicit re-apply,
  uncertain no-replay, owner-loss and reply-gap owner/epoch replacement abort,
  diagnostic retention, fail-closed snapshot decode, answerable Conflict
  Revert, clean/partially dirty authority rebase, exact outbound keys, strict
  enum metatypes, later-key partial-failure results, and an independent
  monospace save/readback with untouched interface font and external rebase.
- `qindaqt.appearance-page` — offscreen Controls scene: focused-destination
  navigation, theme click selection/gating, installed-font and wallpaper
  selection wiring, tokenized font selectors and checked-only hinting emphasis,
  one shared Settings1 action row, per-key result truth, and accessible roles.
- `qindaqt.appearance-monospace-catalog` — a real offscreen GUI font
  inventory, shipped default validation and Apply, proportional-font rejection,
  and visible invalid readback for a missing saved family.
- `qindaqt.appearance-monospace-page` — real keyboard editing of the installed
  monospace picker at 420×320, typed unknown-family forwarding, independent
  interface draft, sample, and confirmed-versus-draft readout.

The existing `qindaqt.font-settings-bridge` and `qindaqt.native-theme` tests
prove that a confirmed `fonts.monospaceFamily` snapshot reaches the font
coordinator and that Qt's `FixedFont` reads the independent family. The
Appearance model test proves this route commits that exact key alone.
- `qindaqt.appearance-window-decoration-controller` — Aurorae discovery,
  stable selection identities, exact KWin library/theme persistence, unrelated
  key preservation, QindaQt-theme cleanup, and one reload request per apply.
- `qindaqt.appearance-window-decoration-page` — installed-decoration card and
  apply wiring plus the rule that foreign selections hide QindaQt's inapplicable
  preview and controls; both fifteen-style menus and every ADR-0264 title-bar
  option row forward their tokens to the draft.
- `qindaqt.decoration-button-styles` and `qindaqt.decoration-title-options` —
  the named styles' pixels (the shipped ones against their old painters) and
  the option tokens, resolution and layout behind these rows (ADR-0264).

Production application is defined by [ADR-0078](../adr/0078-own-wallpaper-surfaces-in-the-shell.md).
`qindaqt.shell-wallpaper-controller` proves the whole chain over a private
Settings1 service: a fresh profile's default-layer bundled identity reaches
one background window per screen, live commits of a custom path and fit mode
reconcile onto the same surfaces, and an explicit empty choice clears the
source without window churn.

The route also inherits the settings-app offscreen and unknown-route gates.
`qindaqt.settings-app-desktop-identity` proves the built executable embeds the
installed `org.qindaqt.Settings` desktop identity and declares it before any
window construction. Settings1 client/service suites remain the authority for
the generic transport and persistence boundary.
`qindaqt.settings-app-route-construction` creates both startup intents with
both bounded route models and a deliberately unavailable private bus, then
requires exactly one page Loader to remain active.
`qindaqt.settings-app-installed-routes` installs the bounded Appearance runtime
component into a clean prefix and launches both routes with host display,
Wayland, QML-import, and library-path overrides removed.

The bundled artwork follows QindaQt's [Mineral Light visual identity](../shell/visual-identity.md).
