# QindaQt Settings — Appearance route

`qindaqt-settings --page appearance` is the first-party Appearance settings
surface. It is one modular route beside the Notifications route inside the
ordinary `qindaqt-settings` Qt Quick application. The domain module
`src/apps/settings/appearance` owns validated appearance values, the QST-1
preview projection, and the Settings1-backed route model; the executable owns
only the additive route seam. The durable composition decisions are recorded
in [ADR-0028](../adr/0028-compose-appearance-settings-through-settings1.md).
The route keeps its transaction state machine separate from preview projection
and divides presentation into dedicated Themes, Wallpaper, and Fonts
destinations. The destinations share one route-level draft and Apply/Revert
boundary; none imports persistence or platform APIs.

## What the route offers

One page covers the appearance preference set stored through Settings1:

| Group | Controls | Settings1 keys |
| --- | --- | --- |
| Themes | Installed-theme previews, dark/light/system scheme preference, and the toolkit card showing the exact QPalette ordinary Qt applications receive for the previewed theme | `appearance.theme`, `appearance.colorScheme` |
| Wallpaper | Bundled previews, native image chooser or local path, and scaled/centered/tiled mode | `appearance.wallpaper`, `appearance.wallpaperMode` |
| Fonts | Installed-family picker with a live sample, size slider (6–36 pt), antialiasing, hinting, and subpixel choices | `fonts.family`, `fonts.pointSize`, `fonts.antialiasing`, `fonts.hinting`, `fonts.subpixelOrder` |

Display scale belongs to the separate **Display** route, which owns the live
output configuration. Appearance offers a direct route action rather than a
second, stored-only scale control.

The toolkit card is presentation-only: the projection is the same
QST-token-to-QPalette mapping the QPA platform-theme plugin applies
(ADR-0115), so the page shows the combined theme truth — QindaQt surfaces
paint from tokens, stock Qt applications follow the platform theme with
palette, fonts, and icons live — without giving the route write access to
any Qt style state.

The page is QST/Controls-only: QindaQt.Controls primitives, QST-1 semantic
roles, `Accessible` names/descriptions/roles on every control, radio
semantics for the scheme and enum choices, an explicit initial focus on the
first theme card, and a visible focus chain through the draft action row. A
single horizontal tab bar above the form selects Themes, Wallpaper, or Fonts
at every width, so Appearance does not introduce a second vertical navigator
beside the Settings Center's route sidebar. The form has a visible vertical
scrollbar, Page Up/Page Down and Ctrl+Home/Ctrl+End scrolling, and automatic
focus reveal; every forward and reverse Tab stop stays inside the compact
420×320 viewport.

## Truthful state surface

The route model projects the same Loading/Ready/Saving/Conflict/Unavailable
truth as the DND controller, extended to a draft workflow:

- **Draft** — edits accumulate locally and are validated immediately
  (installed theme, non-empty family, 6–36 pt, 0.5–3.0 scale, no embedded
  NUL). Invalid fields expose per-key error text and disable Apply; they never
  reach Settings1.
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
3. discover bundled PNG wallpapers from `$XDG_DATA_DIRS/qindaqt/wallpapers` and the installed prefix, with earlier roots winning duplicate file names; choosing one writes its portable `qindaqt:<basename>` identity while a saved custom path or empty choice remains unchanged until the user edits and applies it;
4. merge every theme directory from the same search contract as the text
   editor (`$XDG_DATA_DIRS/qindaqt/themes`, then beside the installed
   executable; `--theme-directory` prepends a developer path). Earlier
   directories win duplicate IDs, while unique built-ins remain present; an
   invalid theme fails closed and no themes exits 3 instead of rendering
   token-less controls;
5. bind the engine-owned `QindaQt.Tokens` singleton, hand it to the model,
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

- The Settings window never mutates compositor or shell surfaces directly. After Apply publishes a confirmed Settings1 snapshot, the production shell background controller adopts the installed or custom wallpaper on every output.
- Font families are listed from the local Qt font database for selection. The
  persisted value remains validated text because first-party session bootstrap
  remains the consumer that applies it before application construction.
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
  enum metatypes, and later-key partial-failure results.
- `qindaqt.appearance-page` — offscreen Controls scene: focused-destination
  navigation, theme click selection/gating, installed-font and wallpaper
  selection wiring, tokenized font selectors and checked-only hinting emphasis,
  one shared action row, per-key result truth, and accessible roles.

Production application is defined by [ADR-0078](../adr/0078-own-wallpaper-surfaces-in-the-shell.md).

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
