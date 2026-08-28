# QindaQt Settings Center

`qindaqt-settings` is the first-party QST-1/Controls navigation shell for
modular settings routes. S1 contains two real routes: **Notifications** and
**Appearance**. The shell owns route identity, selection, responsive
presentation, and navigation accessibility. Each route continues to own its
domain model, service scope, page state, and mutations.

The durable ownership choice is [ADR-0048](../adr/0048-settings-center-navigation-and-route-ownership.md).
Appearance behavior remains documented on the
[Appearance route](appearance-settings.md); notification quieting and its live
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
Notifications or Appearance component. It is not a QML URL, plugin path, or
service locator.

The public command accepts only `--page notifications` and
`--page appearance`. Unknown, noncanonical, path-like, or otherwise hostile
values exit 2 before any settings transport, route model, or QML root is
constructed. Registry lookup also rejects unknown runtime selection without
changing the active or previous route.

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

`SettingsRouteHost` instantiates exactly one active page. Wide and compact
hosts coexist so the window can cross the responsive threshold, but the
inactive host's three Loaders are all inactive. Switching layouts or routes
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
- Escape returns focus to the active visible route tab;
- Ctrl+1 selects Notifications and Ctrl+2 selects Appearance;
- Alt+Left selects the immediately previous route; and
- the platform Quit shortcut closes the ordinary application window.

The navigation containers expose `PageTabList`; each route exposes `PageTab`,
an accessible name/description, and truthful selected state. Unavailable tabs
are disabled and describe their unavailability, while direct activation of an
unavailable registered route presents an accessible alert instead of content.
Each domain page remains responsible for its internal focus cycle and control
semantics.

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
  paths, PageTab semantics, selected state, and fail-closed alerts;
- the CLI row rejects ordinary unknown, uppercase, parent-path, and nested-path
  startup intents with exit 2 and the exact diagnostic;
- the missing-theme poison removes every generic data directory and requires
  exit 3 before QML construction instead of token-less presentation;
- construction starts both route intents against an absent private bus and
  requires each complete root to remain resident; and
- the installed row stages only `SettingsAppearanceRuntime`, removes host
  display/Wayland/QML/library overrides, proves both routes from the relocated
  prefix, and repeats hostile-intent rejection.

This is an offscreen software-renderer and sanitized package boundary. It does
not claim live AT-SPI, compositor focus, screen-reader traversal, platform-
service pages, search, deep links beyond the two fixed IDs, per-route process
isolation, a nested-session screenshot matrix, or physical DPI/input behavior.
