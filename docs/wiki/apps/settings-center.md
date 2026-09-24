# QindaQt Settings Center

`qindaqt-settings` is the first-party QST-1/Controls navigation shell for
modular settings routes. Its registry currently contains **21 routes**, in
stable order: Notifications, Appearance, Display, Network, Customize, Audio,
Bluetooth, Power, Clipboard, Color, Accessibility, Input, Streaming,
Date & time, Windows & workspaces, Default applications, About this computer,
Startup applications, Screen saver, Login screen, and Voice. The shell owns
route identity, selection, responsive presentation, and navigation
accessibility. Each route owns its domain model, service scope, page state,
and mutations. Appending a route must preserve existing indices and digit
shortcuts.

The durable ownership choice is [ADR-0048](../adr/0048-settings-center-navigation-and-route-ownership.md).
The all-route construction witness and its limits are [ADR-0250](../adr/0250-require-active-loader-witness-for-every-settings-route.md).
Ctrl+K search over routes and their sub-pages is [ADR-0257](../adr/0257-search-settings-routes-through-a-qindatk-command-palette.md).
Route behavior is documented in the corresponding [Appearance](appearance-settings.md),
[Display](display-settings.md), [Network](network-settings.md),
[Customize](customize-settings.md), [Audio](audio-settings.md),
[Bluetooth](bluetooth-settings.md), [Power](power-settings.md),
[Clipboard](clipboard-settings.md), [Color](color-settings.md),
[Accessibility](accessibility-settings.md), [Input](input-settings.md),
[Streaming](streaming-settings.md), [Date & time](datetime-settings.md),
[Windows & workspaces](windows-settings.md),
[Default applications](default-applications.md),
[About this computer](about-this-computer.md),
[Startup applications](startup-settings.md),
[Screen saver](screensaver-settings.md),
[Login screen](login-screen-settings.md), and [Voice](voice-settings.md) pages.
Notifications includes Do Not Disturb, Quiet Hours, and per-application
mute/sound controls; their shell policy is documented under
[notification presentation](../shell/notification-presentation.md).
The [21-route completeness inventory](../reference/settings-completeness.md)
separates registered pages from effective controls and outstanding gaps.
Route construction or installed-package success does not establish physical
Bluetooth pairing, battery/lid behavior, color calibration, network radio
state, or remote audio audibility on a particular host.

## Route boundary

The internal `QindaQt::SettingsNavigation` library contains three cohesive
types:

| Type | Authority |
| --- | --- |
| `SettingsRoute` | Bounded stable ID, closed component kind, localized title/description/category, icon name, availability truth, and optional search keywords and deep-link destinations (ADR-0257) |
| `SettingsRouteRegistry` | At most 64 valid unique descriptors in deterministic insertion order |
| `SettingsNavigationController` | Active/previous route, index traversal, QML-safe descriptor projection, and rejected-selection signal |

Route IDs are 1–64 lowercase ASCII alphanumeric, hyphen, or underscore
characters and must begin with an alphanumeric character. Titles, descriptions,
icons, categories, and unavailability diagnostics have independent bounds.
An unavailable descriptor must have a nonempty reason; an available descriptor
must not hide one. The closed component kind maps to one of the 21
compiled route components, from Notifications through Voice. It is not a QML
URL, plugin path, or service locator.

The public command accepts `--page <id>` for any ID in the default
registry. `--list-routes` emits those IDs in registry order for package
verification; it does not instantiate route QML. The private
`--route-construction-probe` exits after the requested active route Loader
reports a real page or an intentional unavailable diagnostic. The installed
`org.qindaqt.Settings.desktop` entry declares `appearance` and `display`
desktop actions whose `Exec` lines open those exact routes.
Unknown, noncanonical, path-like, or otherwise hostile values exit 2 before any
settings transport, route model, or QML root is constructed. Registry lookup
also rejects unknown runtime selection without changing the active or previous
route.

## Composition and lifetime

The process publishes one complete QST-1 generation before constructing the
navigation root. A missing/invalid theme catalog or unavailable Tokens plugin
therefore exits 3 instead of rendering partially initialized Controls.

Notifications and Appearance each own one scoped `SettingsClient` and one
independent `QtSettingsTransport`, although both transports use the session
bus. This separation is required: client request tokens are local sequences
that begin at the same value, so sharing one transport could route a matching
owner/token reply to both clients. The two domain models live for the process
and retain their truthful state when the user changes pages. QML receives only
their QObject projections and never imports transport or Settings1 authority.
The Notifications route projects the Do Not Disturb controller, quiet-hours
schedule, and application policy model over one scoped Settings1 client. Its
scope contains the four `services.doNotDisturb*` keys and
`services.notificationPolicies`; each key is schema-defined with a default, so
the route receives one exact-owner, atomic baseline. A schedule edit, DND edit,
or per-application edit therefore shares one owner and one token sequence
rather than racing another client (ADR-0212, ADR-0255). The application model
receives desktop-entry identities from the public ApplicationCatalog scanner,
including hidden entries, and retains configured rules for apps that are no
longer installed so users can remove them. QML receives display names and
canonical desktop IDs only; it does not scan XDG paths or read Settings1.

The per-app persisted value is one JSON object, keyed by canonical desktop IDs
without `.desktop`. Every rule has exactly `muted` and `soundEnabled` Boolean
fields. Each stored rule retains both Boolean fields; a rule with both values
false is omitted, so an empty object preserves current behavior.
The strict shared codec rejects malformed or unknown records as a whole. UI
edits show only the last confirmed values. A write becomes confirmed only
after Applied and a same-owner, same-epoch snapshot at or above the returned
revision exactly matches the requested object. Same-lineage snapshots below
that floor leave the write pending and trigger another read; a four-second
readback deadline or loss of owner/epoch authority retires it as uncertain,
without replay. A qualifying snapshot with different values reports conflict.
Refusal, conflict, uncertainty, or malformed saved data never becomes
optimistic policy; unavailable and uncertain states retain confirmed values and
expose a refresh action where appropriate. The virtualized application list
traverses mute then sound for each row. Stable first/last focus endpoints
position the ListView before transferring focus, so Tab and Backtab continue
to work when the endpoint delegate is outside the viewport.

The existing quiet-hours and DND controls follow the scoped
client's write-admission signal as well as its write-in-flight state: a
same-owner refresh temporarily disables DND and schedule controls even while
the retained client state says Ready. Each Switch restores its confirmed-value
binding after activation, so an unadmitted or refused click cannot remain
visually On. The controls consume only their own commit outcomes. A schedule edit stays
on its last confirmed value while saving; after an Applied reply it waits for
an exact-owner, same-epoch read at or above the result revision. A refused,
conflicting, uncertain, or unconfirmable result is shown beside quiet hours
without moving its controls or replaying the request. Owner replacement retires
pending schedule intent immediately. Do Not Disturb remains a separate manual
switch: the schedule never writes its key, and neither control borrows the
other control save result. Both DND and the schedule are popup-interruption
reasons: low and normal banners are held, while critical presentation still
requires the independent privacy gate to allow it. Neither is the separate
notification-service disable-all policy, and neither changes lock-screen
privacy or rewrites the manual DND key on a schedule. The focused
`qindaqt.settings-notification-page-admission` offscreen test loads this
actual route with production controllers and a private Settings1 transport;
it covers a same-owner refresh, pointer mute, keyboard sound activation,
refusal, confirmed-value rollback, and unchanged readback without touching live
user settings. The presenter and repository suites separately prove policy
admission and disk round-trip.

Network owns one public Qt Network transport, `NetworkClient`, and
`NetworkSettingsModel` for the process lifetime. It does not share the
Settings1 transport/token domain and never imports the private Network service,
libnm, or credential handling. The Network page receives only its route model;
the complete authority and operation contract is in [Network
Settings](network-settings.md).

Customize owns a separate Settings1 client for `panels.layoutProfile`, an
audited profile/manifest catalog, a profiles store adapter, and one public
customization-editor session. Its QML composition is engine-singleton scoped,
so responsive host reconstruction and route changes retain the draft and
lease. Pending application-close truth is window-owned rather than host-local,
so either responsive host reconstructs the same unresolved modal. It shares no
request tokens or editing lease with another route. The
[Customize route](customize-settings.md) defines its gesture, persistence,
conflict, and failure truth.

Audio owns one public Audio transport, `AudioClient`, and
`AudioSettingsModel` for the process lifetime. It never imports the private
Audio service, WirePlumber, or PipeWire. The Audio page receives only its
route model; the complete authority and operation contract is in
[Audio Settings](audio-settings.md).

Bluetooth owns one public Qt Bluetooth transport, `BluetoothClient`, and route
model for the process lifetime. The route model owns at most one discovery
lease and releases it on route departure or window close. It admits adapter
power, discovery, and paired-device connect/disconnect only from exact
owner/epoch/revision truth; pairing, trust, untrust, and removal remain outside
Settings. The [Bluetooth route](bluetooth-settings.md) defines its fencing,
lease, and BlueZ authority boundary.

Power owns one engine-singleton composition of the public Qt Power transport,
`PowerClient`, and `PowerSettingsModel`. It projects bounded supply,
profile/hold, and brightness truth, admits profile and debounced keyboard-
brightness mutations from exact lineage, and contains no session action. The
[Power route](power-settings.md) defines its fencing and authority boundary.

Clipboard owns independent public Settings1 and Clipboard1 clients in its
route-local QML composition. It projects only the history preference, bounded
count/capacity, and exact lineage needed for confirmed all-history clearing;
clipboard descriptors and content never cross into QML. The
[Clipboard route](clipboard-settings.md) defines its privacy, conflict,
no-replay, and content-authority boundary.

Color owns one engine-singleton composition of the public Qt Display
transport, a Display1 client, a Settings1 client scoped to
`displays.colorAssignments` with its own transport, the C1 assignment store,
the C1 discovery/import provider over XDG-derived roots, and the route model.
It projects the live output inventory, the persisted assignment document, and
the discovered profile catalog; assignment intents apply through the store's
conflict/no-replay truth and no compositor application exists. The
[Color route](color-settings.md) defines its fencing and authority boundary.

Accessibility owns one independent Settings1 transport and a client scoped to
exactly the four consumed accessibility keys, constructed by the executable
like Appearance and passed to QML as its route model. It edits a draft and
applies per key from fresh snapshots; the reserved `accessibility.screenReader`
key is never scoped. The [Accessibility route](accessibility-settings.md)
defines its truth, no-replay, and reserved-key boundary (ADR-0128).

Windows & workspaces owns one route-local composition (like Clipboard): a
Settings1 transport and a client scoped to exactly the four live
`windowManagement.*` keys, passed to QML as its route model. It edits a draft
and applies per key from fresh snapshots; the reserved
`windowManagement.sessionRestore` key is never scoped. The
[Windows & workspaces route](windows-settings.md) defines its boundary
(ADR-0210).

`SettingsRouteHost` instantiates exactly one active page. Wide and compact
hosts coexist so the window can cross the responsive threshold, but every
Loader in the inactive host stays inactive. The Accessibility Loader
additionally requires its route model; a host composed without one shows the
explicit unavailable notice instead of binding a page to null. The
Windows & workspaces loader lives in its own file and, like Input, binds the
page to the route's composition singleton inside its component. Switching
layouts or routes cannot duplicate a page, its focus side effects, or its
settings bindings.
Unknown component keys and unavailable descriptors select one explicit
`DegradedNotice`; no route falls back to another domain page.

## Responsive interaction

At widths of 540 logical pixels or greater, a two-column view presents a
224-pixel navigation sidebar and the active route. The sidebar groups the
unchanged route registry into General, Personalization, and Hardware headings;
this is a presentation sort only and does not alter route IDs, history, or
shortcuts. Its muted Settings label identifies the navigation region while the
active route keeps the one prominent page title. The full grouped route list
scrolls vertically in a bounded viewport with a scrollbar when needed. Route
selection, keyboard focus and resizing reveal the relevant button; wheel
scrolling otherwise retains the user's position. Below 540 pixels, a compact single-column, horizontally scrollable
PageTabList appears above the active route. Both variants use
only QST-1 semantic roles and QindaQt.Controls presentation. The selected
route tab uses the default foreground on its raised-surface background; the
accent foreground is reserved for text placed on the accent surface. This
keeps selected navigation readable when the theme changes live between light,
dark, and high-contrast palettes. The compact list reveals the selected route
when it is created, selected, or resized, and reveals each tab reached by
Left/Right keyboard focus. Reveal adjustments occur only for those navigation
and viewport events so touch and pointer users can otherwise scroll the list
freely.
The compact horizontal list accepts ordinary vertical wheel input as well as
horizontal touchpad scrolling and has an attached horizontal scrollbar.

The ordinary starting window is 960×680 logical pixels. Its 420×320 minimum
remains supported for compact/offscreen interaction coverage.

The interaction contract is:

- click or Enter/Return activates a route tab;
- Up/Down move within the wide route list; Left/Right move within compact tabs;
- Tab from a route tab enters the active page's declared first focus target;
- Escape returns focus to the active visible route tab. There are two
  exceptions. While the Bluetooth route shows an active pairing prompt with a
  free reply lane, the host Escape shortcut yields to the route's own Escape
  shortcut so the prompt receives its cancel reply. While the search palette
  is visible, Escape closes only the palette. Two enabled identical
  window-context shortcuts would be ambiguous and neither would activate;
- Ctrl+K, or the "Search settings" button in the sidebar or the compact
  header's "Search" button, opens the search palette (see below);
- while an Input shortcut capture is active, the Input route reports that
  state and the shell disables its global navigation, search, Escape, and Quit
  shortcuts. Ctrl+K, Ctrl+digit, and Alt+Left are captured without opening
  search or changing routes; plain Tab still leaves capture;
- Ctrl+1, Ctrl+2, Ctrl+3, Ctrl+4, and Ctrl+5 select Notifications, Appearance,
  Display, Network, and Customize respectively; Ctrl+6 selects Audio in its appended
  sixth position, and Ctrl+7 selects Bluetooth in its appended seventh
  position; Ctrl+8 selects Power in its appended eighth position, and Ctrl+9
  selects Clipboard in its appended ninth position; Ctrl+0 selects Color in
  its appended tenth position; all later routes, from Accessibility (11)
  through Voice (21), have no digit shortcut and are reached from the sidebar
  or compact tab list;
- Alt+Left selects the immediately previous route; and
- the platform Quit shortcut closes the ordinary application window unless
  Bluetooth must first release a discovery lease or Customize owns a dirty
  draft; discovery release completes first, then the same modal discard
  decision as title-bar close and route departure must resolve.

## Search

Settings search ([ADR-0257](../adr/0257-search-settings-routes-through-a-qindatk-command-palette.md))
is QindaTK's `Tk.CommandPalette`, wrapped by `SettingsCommandPalette.qml` and
themed through `QindaQtTheme`. It is the one QindaTK surface in the navigation
shell. With an empty filter it lists every registered route, sectioned General,
Personalization, Hardware and sorted by title like the sidebar, and prints the
route's Ctrl+digit shortcut for the first ten registered routes. Input's five
sub-pages (Mouse & touchpad, Pen & tablet, Keyboard, Shortcuts, Touch) follow
it as "Input › …" entries. Search matches only titles and registered keywords;
route descriptions are not indexed. Matches are ordered by title prefix, then
exact keyword, then label substring, then keyword prefix, then other keyword
substrings. A nonempty filter
puts all matches in one "Results" section because QindaTK groups rows by
section; this keeps the global relevance order visible across the sidebar's
sections. For "screen", Screen saver ranks first by title prefix, Display next
by exact keyword, Login screen next by label substring, then Power and
Streaming by keyword prefix in their existing title order, followed by Input
› Touch because "screen" occurs within its "touchscreen" keyword. "wifi"
leads to Network, "battery" returns Power (the About description's
battery-health text is not searchable), and "shortcuts" leads to Input ›
Shortcuts. Arrow keys move the selection, Enter or a click activates, and
Escape closes. After a route change, focus moves to the new page's declared
first focus target.

The terms and destinations are registry data
(`settings_route_search_metadata.cpp`), not read from the pages. Every route
has keywords, and only Input declares destinations. A destination entry calls
`selectRouteDestination`. Repeating the current request while Input is open
re-delivers it: the controller clears the link and sets it again, and
`InputPage` opens a destination that arrives while it is open. Plain route
entries call `selectRoute`, like the sidebar, so the route's last requested
destination is kept. An unavailable route stays listed as
"Title (unavailable: reason)". Choosing it changes nothing, the same guard as
the sidebar and compact tabs, and it offers no destinations. The palette's
filter field and list come from QindaTK. Its rows have no per-row accessible
description, so the unavailable reason is part of the row text.

The navigation containers expose `PageTabList`; each route exposes `PageTab`,
an accessible name/description, and truthful selected state. Unavailable tabs
remain focusable so assistive technology and Escape-return navigation can
reach their registered diagnostic, but their activation handler is guarded.
A registered route already selected by startup or controller authority presents
an accessible alert instead of content. Each domain page remains responsible
for its internal focus cycle and control semantics.

## Verification and stopping point

Focused selection:

```sh
ctest --test-dir build/dev --output-on-failure \
  -R '^qindaqt\.settings-(route-(registry|search)|command-palette|navigation-(controller|layout|interaction))$'
ctest --test-dir build/dev --output-on-failure \
  -R '^qindaqt\.settings-app-'
```

- registry/controller tests cover hostile bounds, duplicates/capacity,
  component mapping, deterministic order, unknown selection, history,
  traversal, and unavailable truth;
- `qindaqt.settings-route-search` proves that every route has keywords, that
  route order and the ten digit routes are unchanged, that only Input
  declares destinations and that their ids and titles match `InputPage.qml`,
  that hostile keywords and destinations are rejected, and that a repeated
  destination is re-delivered;
- `qindaqt.settings-command-palette` drives the real `Main.qml` with
  `QT_FATAL_WARNINGS=1`. It covers Ctrl+K, "wifi" → Network, "battery" →
  Power with no description-only About match, the cross-section "screen"
  relevance order, the printed Ctrl+digit shortcuts, "shortcuts" → Input ›
  Shortcuts, Ctrl+K and Ctrl+1 captured without invoking shell shortcuts while
  the real Input capture is active, a second destination while Input is open,
  a repeated destination after an in-page change, Escape closing only the
  palette with focus restored, the unavailable route listed but not selected,
  and the 420×320 compact window by keyboard only;
- the offscreen navigation layout and interaction rows prove 720×520 wide
  and 440×360 compact layout, mutually exclusive page construction, route
  switching, shortcut and focus paths, PageTab semantics, selected state, and
  fail-closed alerts; both use `QT_FATAL_WARNINGS=1`, so a QML warning during
  host or route construction aborts the row. The layout target also lists
  Power's Q_OBJECT screen-lock fixture
  header, whose stub preferences the host injects while Color is selected,
  as an explicit source so AUTOMOC generates its Qt meta-object; without
  that registration the target fails to link. The child-process construction
  and installed rows deliberately tolerate `main()`'s absent-bus
  client-unavailability warnings while rejecting QML/Loader warnings;
- the CLI row rejects ordinary unknown, uppercase, parent-path, and nested-path
  startup intents with exit 2 and the exact diagnostic;
- the missing-theme poison removes every generic data directory and requires
  exit 3 before QML construction instead of token-less presentation;
- construction takes the route inventory from `--list-routes`, checks the
  current count of 21 and unique canonical IDs, and launches every registered
  intent under absent private **session and system buses**. Each run must exit
  after its active Loader reaches Ready with a real item, or shows the
  registry-declared unavailable diagnostic. A timeout, exit without an exact
  route-ID witness, or unexpected QML/Loader warning fails. A fake executable
  checks both residence without a witness and a ready witness accompanied by
  a QML warning as focused negative controls;
- the installed row stages only `SettingsAppearanceRuntime` after its
  Login Screen helper build prerequisite, removes host
  display/Wayland/QML/library overrides, withholds its required Appearance QML
  module while the developer tree remains present and requires exit 3, then
  repeats that poison for the Network, Audio, and Accessibility modules, then
  reinstalls and proves all 21 routes, including the Customize catalogs and
  later Date & time, Windows, Startup, Screen saver, Login screen, and Voice
  modules, from only the complete relocated prefix; and
- the same no-borrowing contract is enforced at startup, not only by the
  test: before any engine work the executable preflights its own QML root for
  every directory-resolved route module (the Customize and `*Backend` modules
  are statically linked into the binary and need no directory) and exits 3
  with a typed diagnostic when one is missing, because the QML engine's
  default import path would otherwise resolve a same-named module from the
  system Qt install — the condition that previously let relocated copies
  silently borrow modules instead of failing the poison; and
- the same installed row repeats hostile-intent rejection.

This is an offscreen software-renderer and sanitized package boundary. It does
not claim live AT-SPI, compositor focus, screen-reader traversal, platform-
service pages beyond the compiled routes, search inside a page's controls,
arbitrary deep links,
per-route process isolation, a nested-session screenshot matrix, or physical
DPI/input behavior.
