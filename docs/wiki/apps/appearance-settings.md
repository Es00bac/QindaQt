# QindaQt Settings — Appearance route

`qindaqt-settings --page appearance` is the first-party Appearance settings
surface. It is one modular route beside the Notifications route inside the
ordinary `qindaqt-settings` Qt Quick application. The domain module
`src/apps/settings/appearance` owns validated appearance values, the QST-1
preview projection, and the Settings1-backed route model; the executable owns
only the additive route seam. The durable composition decisions are recorded
in [ADR-0028](../adr/0028-compose-appearance-settings-through-settings1.md).
The route keeps its transaction state machine separate from preview projection
and divides presentation into Theme, Font, and Desktop-preference QML sections;
none of those sections imports persistence or platform APIs.

## What the route offers

One page covers the appearance preference set stored through Settings1:

| Group | Controls | Settings1 keys |
| --- | --- | --- |
| Theme | Installed-theme cards with live QST previews, dark/light/system scheme preference | `appearance.theme`, `appearance.colorScheme` |
| Fonts | Family text field, size slider (6–36 pt), antialiasing switch, hinting and subpixel segmented choices | `fonts.family`, `fonts.pointSize`, `fonts.antialiasing`, `fonts.hinting`, `fonts.subpixelOrder` |
| Wallpaper | Path field (empty = none), scaled/centered/tiled mode | `appearance.wallpaper`, `appearance.wallpaperMode` |
| Display scale | Logical UI scale slider (0.5–3.0) | `appearance.uiScale` |

The page is QST/Controls-only: QindaQt.Controls primitives, QST-1 semantic
roles, `Accessible` names/descriptions/roles on every control, radio
semantics for the scheme and enum choices, an explicit initial focus on the
first theme card, and a visible focus chain through the draft action row. The
form has a visible vertical scrollbar, Page Up/Page Down and Ctrl+Home/Ctrl+End
scrolling, and automatic focus reveal; every forward and reverse Tab stop stays
inside the compact 420×320 viewport.

## Truthful state surface

The route model projects the same Loading/Ready/Saving/Conflict/Unavailable
truth as the DND controller, extended to a draft workflow:

- **Draft** — edits accumulate locally and are validated immediately
  (installed theme, non-empty family, 6–36 pt, 0.5–3.0 scale, no embedded
  NUL). Invalid fields expose per-key error text and disable Apply; they never
  reach Settings1.
- **Preview** — the draft drives one complete published QST generation, so
  the page chrome and every theme card preview stay consistent. When the
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
3. merge every theme directory from the same search contract as the text
   editor (`$XDG_DATA_DIRS/qindaqt/themes`, then beside the installed
   executable; `--theme-directory` prepends a developer path). Earlier
   directories win duplicate IDs, while unique built-ins remain present; an
   invalid theme fails closed and no themes exits 3 instead of rendering
   token-less controls;
4. bind the engine-owned `QindaQt.Tokens` singleton, hand it to the model,
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

- No compositor, display, session, font, or wallpaper mutation. The UI scale
  and wallpaper fields are stored intent; application remains with public
  Display1/Settings consumers in later slices.
- No font discovery: the family field is validated text, not a host font
  catalog.
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

- `qindaqt.appearance-values` — token round trips, canonical decode, typed
  rejections including empty non-empty-schema strings, draft validation, and
  exact shipped-schema key/default/constraint contracts.
- `qindaqt.appearance-preview` — configured-theme precedence, scheme and
  platform fallbacks, complete preview maps for every built-in theme,
  high-contrast QST input, and an exact five-ID inventory including
  `qinda-macos`.
- `qindaqt.appearance-settings-model` — baseline decode, per-key commit
  sequence with fresh-base snapshots, conflict stop/explicit re-apply,
  uncertain no-replay, owner-loss and reply-gap owner/epoch replacement abort,
  diagnostic retention, fail-closed snapshot decode, answerable Conflict
  Revert, clean/partially dirty authority rebase, exact outbound keys, strict
  enum metatypes, and later-key partial-failure results.
- `qindaqt.appearance-page` — offscreen Controls scene: theme-card
  click selection/gating, zero-argument toggle and real text-entry wiring,
  action-row and per-key result wiring, status/error/fallback truth and
  accessible roles, plus full visible forward/reverse compact traversal.

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
