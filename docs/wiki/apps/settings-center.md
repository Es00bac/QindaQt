# QindaQt Settings Center

`qindaqt-settings` is the first-party QST-1/Controls navigation shell for
modular settings routes. It contains nine real routes: **Notifications**,
**Appearance**, **Display**, **Network**, **Customize**, **Audio**, and
**Bluetooth**, followed by **Power** and **Clipboard**. The shell owns route identity, selection, responsive
presentation, and navigation accessibility. Each route continues to own its
domain model, service scope, page state, and mutations.

The durable ownership choice is [ADR-0048](../adr/0048-settings-center-navigation-and-route-ownership.md).
Appearance behavior remains documented on the
[Appearance route](appearance-settings.md); Display behavior is documented on
the [Display route](display-settings.md); Network behavior is documented on
the [Network route](network-settings.md); Customize behavior is documented on
the [Customize route](customize-settings.md); Audio behavior is documented on
the [Audio route](audio-settings.md); Bluetooth behavior is documented on the
[Bluetooth route](bluetooth-settings.md); Power behavior is documented on the
[Power route](power-settings.md); Clipboard behavior is documented on the
[Clipboard route](clipboard-settings.md); notification quieting and its live
settings transaction remain documented under
[notification presentation](../shell/notification-presentation.md).

## Route boundary

The internal `QindaQt::SettingsNavigation` library contains three cohesive
types:

| Type | Authority |
| --- | --- |
| `SettingsRoute` | Bounded stable ID, closed component kind, localized title/description/category, icon name, and availability truth |
| `SettingsRouteRegistry` | At most 64 valid unique descriptors in deterministic insertion order |
| `SettingsNavigationController` | Active/previous route, index traversal, QML-safe descriptor projection, and rejected-selection signal |

Route IDs are 1–64 lowercase ASCII alphanumeric, hyphen, or underscore
characters and must begin with an alphanumeric character. Titles, descriptions,
icons, categories, and unavailability diagnostics have independent bounds.
An unavailable descriptor must have a nonempty reason; an available descriptor
must not hide one. The closed component kind is mapped to the compiled
Notifications, Appearance, Display, Network, Customize, Audio, Bluetooth, Power,
or Clipboard
component. It is not a QML URL, plugin path, or service locator.

The public command accepts `--page notifications`, `--page appearance`,
`--page display`, `--page network`, `--page customize`, `--page audio`, and
`--page bluetooth`, `--page power`, and `--page clipboard`.
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

`SettingsRouteHost` instantiates exactly one active page. Wide and compact
hosts coexist so the window can cross the responsive threshold, but the
inactive host's nine route Loaders are all inactive. Switching layouts or routes
cannot duplicate a page, its focus side effects, or its settings bindings.
Unknown component keys and unavailable descriptors select one explicit
`DegradedNotice`; no route falls back to another domain page.

## Responsive interaction

At widths of 540 logical pixels or greater, a two-column view presents a
200-pixel navigation sidebar and the active route. Below 540 pixels, a compact
single-column PageTabList appears above the active route. Both variants use
only QST-1 semantic roles and QindaQt.Controls presentation.

The interaction contract is:

- click or Enter/Return activates a route tab;
- Up/Down move within the wide route list; Left/Right move within compact tabs;
- Tab from a route tab enters the active page's declared first focus target;
- Escape returns focus to the active visible route tab. The one route-level
  exception: while the Bluetooth route shows an active pairing prompt with a
  free reply lane, the host Escape shortcut yields to the route's own Escape
  shortcut so the prompt receives its cancel reply — two enabled identical
  window-context shortcuts would be ambiguous and neither would activate;
- Ctrl+1, Ctrl+2, Ctrl+3, and Ctrl+4 select Notifications, Appearance,
  Display, and Network respectively; Ctrl+6 selects Audio in its appended
  sixth position, and Ctrl+7 selects Bluetooth in its appended seventh
  position; Ctrl+8 selects Power in its appended eighth position, and Ctrl+9
  selects Clipboard in its appended ninth position;
- Alt+Left selects the immediately previous route; and
- the platform Quit shortcut closes the ordinary application window unless
  Bluetooth must first release a discovery lease or Customize owns a dirty
  draft; discovery release completes first, then the same modal discard
  decision as title-bar close and route departure must resolve.

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
  -R '^qindaqt\.(settings-(route-registry|navigation-controller|navigation-page)|settings-app-(offscreen|rejects-(unknown-route|missing-theme)|desktop-identity|route-construction|installed-routes))$'
```

- registry/controller tests cover hostile bounds, duplicates/capacity,
  component mapping, deterministic order, unknown selection, history,
  traversal, and unavailable truth;
- the offscreen navigation row proves 720×520 wide and 440×360 compact layout,
  mutually exclusive page construction, route switching, shortcut and focus
  paths, PageTab semantics, selected state, and fail-closed alerts; it is
  registered with `QT_FATAL_WARNINGS=1`, so any QML warning emitted while
  constructing the host or a route page aborts the row instead of passing
  silently. The child-process construction and installed rows deliberately
  tolerate `main()`'s absent-bus client-unavailability warnings and are not
  registered as fatal;
- the CLI row rejects ordinary unknown, uppercase, parent-path, and nested-path
  startup intents with exit 2 and the exact diagnostic;
- the missing-theme poison removes every generic data directory and requires
  exit 3 before QML construction instead of token-less presentation;
- construction starts all nine route intents against an absent private bus and
  requires each complete root to remain resident;
- the installed row stages only `SettingsAppearanceRuntime`, removes host
  display/Wayland/QML/library overrides, withholds its required Appearance QML
  module while the developer tree remains present and requires exit 3, then
  repeats that poison for the Network module and the Audio module, then
  reinstalls and proves all nine routes, including the Customize catalogs and
  Bluetooth, Power, and Clipboard modules, from only the complete relocated
  prefix; and
- the same installed row repeats hostile-intent rejection.

This is an offscreen software-renderer and sanitized package boundary. It does
not claim live AT-SPI, compositor focus, screen-reader traversal, platform-
service pages beyond the nine compiled routes, search, arbitrary deep links,
per-route process isolation, a nested-session screenshot matrix, or physical
DPI/input behavior.
