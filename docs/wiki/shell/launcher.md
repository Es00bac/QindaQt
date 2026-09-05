# Launcher

The launcher presents installed applications for browsing and activation.
This page records the pure L0 model, the L1 production adapters, and the L2
shell composition: installed-application scanning, pinned/recent persistence,
bounded execution, the compiled `QindaQt.Shell.Launcher` applet module, and its
production panel host.

The implementation lives in `src/shell/launcher`. The L0 model (namespace
`QindaQt::ShellLauncher`, target `qindaqt_shell_launcher`) remains a pure Qt
Core library: no Qt Gui, Qt Quick, Qt DBus, or KDE frameworks, and no
filesystem, environment, session bus, or process access. The L1 adapters
(namespace `QindaQt::Shell::Launcher`, targets
`qindaqt_shell_launcher_runtime` and the `QindaQt.Shell.Launcher` QML module)
own that platform reach behind dedicated seams. The runtime target exposes no
Qt Gui/QML/Quick dependency, so scanner, execution, persistence, and controller
consumers remain `QCoreApplication` processes; only the separately linked QML
module and offscreen QML test initialize a GUI application. The model boundary
is accepted in [ADR-0042](../adr/0042-launcher-model-without-execution.md); the
L1 execution/activation boundary in
[ADR-0062](../adr/0062-bound-launcher-execution-behind-injected-seams.md).

## Values and validation

`DesktopEntryParser` turns one desktop-entry document into a validated value
or a typed error (`DesktopEntryErrorCode`). The accepted subset is the
freedesktop desktop-entry keys the launcher presents: `Type` (only
`Application` is launchable), `Name`, `GenericName`, `Comment`, `Icon`,
`NoDisplay`, `Hidden`, `Categories`, `Keywords`, and `Actions` with their
`[Desktop Action <id>]` groups. Unknown keys and locale-suffixed variants such
as `Name[de]` are ignored without decoding their payload, so an extension's
escape grammar cannot invalidate a document. Every key first requires a
non-empty ASCII key name and, when present, a complete
`lang[_COUNTRY][.ENCODING][@MODIFIER]` locale suffix with non-empty ordered
components. Recognized groups reject duplicate keys/groups, booleans accept
only `true` or `false`, whitespace around `=` is ignored, escaped semicolons
remain inside list items, and action IDs use the bounded ASCII key-name grammar.

Documents are hostile input. The parser and catalog enforce fixed ceilings
declared in `launcher_bounds.h` (QString document code units, field lengths,
keyword, and action counts). Violations produce typed errors — never
exceptions, partial entries, or unbounded growth.

`ApplicationCatalog::build` is a total function over source documents:

| Input | Result |
| --- | --- |
| Valid visible document | One `ApplicationEntry`, id = source id |
| `Hidden=true` or `NoDisplay=true` | Identity claimed, then silently skipped (normal deletion/visibility marker) |
| Parse failure | `InvalidDocument` diagnostic |
| Repeated source id | `DuplicateEntryId` diagnostic; first document claims the id even when hidden or invalid |
| Entry or source ceiling | `EntryLimitReached`/`SourceLimitReached` diagnostic; build stops |

Entries are ordered case-insensitively by display name, then id. The order is
stable for identical input. Diagnostics are bounded; overflow sets a
truncation flag instead of growing unbounded.

## Categories, search, and launch intents

`LauncherCategoryModel` maps XDG categories onto twelve fixed QindaQt groups
in a stable presentation order; unmapped entries land in Other. The mapping is
locale-independent by design.

`LauncherSearchRanker` matches the normalized (trimmed, whitespace-collapsed)
query case-insensitively and ranks by match quality — name prefix, name word
start, name substring, keyword prefix, keyword substring, generic name, then
comment — breaking ties by name, then id. Blank queries are a typed error, not
a request for the full catalog; callers wanting the default listing browse the
catalog or categories directly.

Activation resolves through `ApplicationCatalog::makeLaunchIntent`, which
returns a `LaunchIntent`: entry id, optional action id, and display values.
Keyboard and pointer activation share this one resolver. The intent carries no
command line and no execution path; starting a process is a later adapter
boundary, so a hostile document cannot reach execution logic through the
model.

## Pinned, recent, and presentation

`PinnedApplications` is an ordered identity list (ceiling 16) with explicit
pin/unpin/move outcomes. `RecentApplications` is a bounded (ceiling 8)
most-recently-used list where recording an existing id moves it to the front
and the oldest id is evicted. Neither pure model persists anything; the L1
`LauncherPersistenceController` composes them with Settings1 without moving
transport or storage authority into L0.

`LauncherPresentationModel::build` projects a catalog, pinned, and recent
state into ordered sections and items:

- no catalog yet → `Loading`;
- catalog with diagnostics → `Degraded` (remaining valid entries still
  present);
- catalog without entries or diagnostics → `Empty`;
- otherwise → `Ready` with Pinned, Recent, and category sections; a valid
  query collapses the surface to one search-results section.

Focus order is exactly section order then item order (`itemAt`), and every
item carries display and accessible values (name, icon, a non-empty description)
so a QML adapter can render keyboard- and screen-reader-ready surfaces without
owning policy. Sections expose stable label/category identities rather than
hard-coded English; localization belongs to that adapter. A valid search always
publishes one SearchResults section, including the no-match state. Stale
pinned/recent ids that are no longer visible entries silently disappear from
the projection.

## L1 production adapters

The L1 slice adds the production adapters in `src/shell/launcher`, each behind
its own seam (target `qindaqt_shell_launcher_runtime`). The accepted boundary
is [ADR-0062](../adr/0062-bound-launcher-execution-behind-injected-seams.md).

### Installed-application scanning

`ApplicationScanner` reads the `applications/` tree of each injected data
root — the composition root resolves the XDG data-home/data-dirs list; the
scanner never reads the environment implicitly. Subdirectories map to
desktop-entry ids with `/` → `-`; earlier roots win identity precedence,
matching the catalog's first-claim rule. Every traversed directory and desktop
entry is canonicalized and must remain beneath its injected data root;
escaping links, dangling links, FIFOs, devices, sockets, and every other
non-regular file are diagnosed without being opened. Per-file byte and
document ceilings are enforced before decoding, and reads request at most the
byte ceiling plus one sentinel byte so concurrent growth stays bounded. The
total scanned file count is bounded by `maxSourceDocuments`. A debounced
(200 ms) `QFileSystemWatcher` over the
watched directories and desktop files triggers a synchronous rebuild; every
completed rebuild publishes a monotonically increasing generation with the
`catalogChanged` signal so consumers fence stale reactions. Unreadable roots
or files and oversized documents produce scanner-level diagnostics (bounded,
truncation-flagged) and set the surface's `Degraded` truth together with the
catalog's document diagnostics. Root and `applications/` metadata checks are
errno-aware: only a syscall-confirmed `ENOENT` is normal absence; denied
ancestor traversal or any other indeterminate status degrades with a bounded
diagnostic. Existing unreadable data roots and dangling top-level
`applications` links also degrade. The raw text and absolute path of each
winning document are retained (bounded) exclusively for the execution adapter.

### Pinned/recent persistence

`LauncherPersistenceController` borrows the public Settings1 client scoped to
the documented key set:

| Key | Value |
| --- | --- |
| `panels.launcherPinned` | Ordered desktop-entry ids, at most 16 |
| `panels.launcherRecent` | Most-recent-first desktop-entry ids, at most 8 |

Only desktop-entry ids are ever stored. Stored values are validated on every
snapshot: a non-list, non-string element, an invalid id, a duplicate, or an
over-ceiling count poisons the whole key, which is then treated as absent
with visible degraded truth — partial lists never enter the models. Semantics
follow ADR-0012: a mutation applies to the live model and commits
immediately; a confirmed rejection (including `UnknownKey`) reverts the model
to the last confirmed value and keeps the reason visible until the next
explicit write; an uncertain commit is never replayed and converges through
the resync snapshot; Settings1 owner or transport loss clears pinned/recent
truth and refuses new writes. The client serializes writes, so a mutation during an
in-flight write is refused as `Busy` rather than queued.

Registering the launcher key set in the Settings1 schema is a
settings-schema authority change owned outside this lane. Until that lands,
the production service answers `UnknownKey`; the controller fails closed and
reports the refusal truthfully (covered by a focused test).

### Bounded execution

`LaunchExecutor` turns an L0 launch intent into a process start with no shell
interpolation. Every launch first resolves through the catalog's single
`makeLaunchIntent` resolver — an entry the catalog does not publish (unknown,
hidden, or shadowed) is refused before any execution planning. The execution
keys (`Exec`, `Terminal`, `Path`, `DBusActivatable`) are re-extracted under
fixed ceilings from the scanner-retained raw document. A desktop action
contributes only its own `Exec`; it inherits entry-level `Terminal`, `Path`,
and `DBusActivatable`, while action-local lookalike keys stay opaque together
with locale variants and extension keys. `Exec` expansion follows the desktop-entry field
codes: `%c`/`%k` expand in place, `%i` becomes the two-argument `--icon` form
only as a standalone token, file/URL and deprecated codes drop as whole
tokens, and embedded or unknown codes are a typed refusal. Output argv is
capped in count and total size.

Dispatch then follows the entry's declared surface:

| Entry | Route |
| --- | --- |
| `DBusActivatable=true` | `org.freedesktop.Application.Activate` / `ActivateAction` on the session bus through the injected `LaunchActivator`; dispatch and completion are separate truths |
| `Terminal=true` | The injected terminal command prefix (composition wires the QindaQt Terminal launch policy), or a truthful refusal when unwired — never a shell fallback |
| otherwise | The injected `LaunchSpawner`; production uses `QProcess::startDetached` with the entry's `Path` and a sanitized environment |

The child-environment allowlist forwards session identity (`HOME`, `PATH`,
`XDG_RUNTIME_DIR`, `XDG_DATA_DIRS`, `XDG_CONFIG_DIRS`, `XDG_SESSION_*`,
`XDG_CONFIG_HOME`, `XDG_DATA_HOME`, `XDG_CACHE_HOME`, `XDG_STATE_HOME`,
`XDG_CURRENT_DESKTOP`), locale (`LANG`, `LC_ALL`, `LC_*`), display
(`WAYLAND_DISPLAY`, `DISPLAY`, `XAUTHORITY`), authentication-agent socket
(`SSH_AUTH_SOCK`), `DBUS_SESSION_BUS_ADDRESS`, and Qt platform
selection (`QT_QPA_PLATFORM`, `QT_SCALE_FACTOR`) — nothing else, including
QindaQt's own development overrides. Activation (startup-notification)
tokens are a later slice; the D-Bus platform-data map is empty today. Tests
never start real applications: the seams are interfaces, and the only real
children any test starts are the inert `/bin/true` and `/bin/false` fixtures.

### Compiled QML applet

The compiled module `QindaQt.Shell.Launcher` (`LauncherApplet` +
`LauncherSection`) renders one icon-only summary button and a non-modal browser popup —
search field, category sections, pinned/recent rows, and search results —
over the shell-private `LauncherAppletController`, using `QindaQt.Controls`
primitives and QST-1 tokens. The controller projects the L0 presentation
model into bounded values; QML owns only presentation, localization of the
stable section identities, keyboard traversal (Tab into the field, Down into
the results, flat Up/Down across sections in focus order, Return/Space to
activate, Escape to close), and complete accessible names/roles/states.
Every item's accessible description comes from the L0 model, so state meaning
never depends on color or position. Scanner, execution, and persistence
diagnostics render as bounded three-line accessible alerts. Before QST tokens
or a controller exist, the applet constructs only its disabled summary; it
does not evaluate controller properties or construct token-dependent browser
content. The panel requests `start-here-kde` with symbolic-class fallback and
uses the typed icon placeholder when neither asset resolves. “Applications”
remains the button's accessible name and the popup heading, but is never
painted into the panel. The compact summary is 32 by 28 logical pixels in both
orientations.

The manifest (`data/applets/launcher.json`) requests `applications.launch`;
the grant gates activation in the controller, and the entry point
`qindaqt.applets.launcher` is registered in the audited first-party
registry. The preview injects no controller and shows a disabled,
deterministic fallback. The production dispatcher receives only the
shell-owned controller and renders the same compiled module for a resolved
launcher instance.

## Production shell composition

`LauncherAppletComposition` is the shell-private production boundary. The
shell composition root resolves XDG data roots from an explicitly supplied
environment snapshot and home directory, then constructs the scanner without
giving that adapter implicit environment access. The composition borrows the
shell's one public Settings1 client, owns the production process spawner and
session-bus activator, and admits activation only when the audited manifest,
built-in registry, and capability policy grant `applications.launch`.
Settings1 owner loss clears pinned/recent identity truth; an unavailable or
unregistered schema never becomes cached authority.

The runtime panel factory injects the resulting controller into each panel.
`BuiltinAppletContent` renders `QindaQt.Shell.Launcher` for a resolved launcher
and uses a bounded ancestor lookup solely to cross the generic applet-chip
presentation boundary. The summary button opens the popup with search focus;
Escape closes it, pointer and keyboard activation share the controller path,
and accessible button, field, list, state, and alert semantics come from the
compiled applet. Preview recognizes the launcher profile id without performing
runtime resolution and deliberately supplies a null controller, producing the
same disabled deterministic fallback as before.

Each stock profile places exactly one launcher instance in its launcher slot.
The production terminal-command prefix remains deliberately unwired, so
`Terminal=true` entries refuse rather than inventing a terminal policy.
`LauncherAppletRuntime` ships the shell, launcher manifest/profile/policy/theme,
the compiled Launcher module, and its Controls/Tokens loader closure as one
relocatable install component.

## Focused tests

```sh
ctest --test-dir build/dev -R '^qindaqt\.launcher-' --output-on-failure
```

| Test | Scope |
| --- | --- |
| `qindaqt.launcher-desktop-entry-parser` | L0 parser hostile corpus. |
| `qindaqt.launcher-application-catalog` | L0 catalog identity claiming, duplicates, bounds. |
| `qindaqt.launcher-category-model` | Fixed category mapping and order. |
| `qindaqt.launcher-search-ranker` | Ranking, normalization, ties. |
| `qindaqt.launcher-pinned-recent` | L0 pinned/recent model bounds and outcomes. |
| `qindaqt.launcher-presentation` | L0 presentation states and focus order. |
| `qindaqt.launcher-application-scanner` | Fixture trees, precedence, subdirectory ids, escaping application-tree links, FIFO/non-regular refusal, hostile/unreadable/oversized entries, denied ancestor traversal versus confirmed absence, hidden shadowing, watcher refresh, generation fencing, document retention, deterministic order. |
| `qindaqt.launcher-execution` | Entry/action key scope, quoting, field-code expansion/refusal, no-shell-interpolation, output ceilings. |
| `qindaqt.launcher-executor` | Intent fencing, spawner/activator seams, entry-policy inheritance by actions, hostile action-key inverse control, terminal policy routing/refusal, failure truth, inert fixture spawns, environment sanitization. |
| `qindaqt.launcher-persistence` | Settings1 round trips, hostile stored values, conflict revert, `UnknownKey` fail-closed, unchanged-authority convergence after uncertain commits without replay, transport loss, write serialization, bounds. |
| `qindaqt.launcher-settings-contract` | Shipped Settings1 schema and real private-bus transport, pinned/recent disk persistence, new service owner/epoch recovery, and fresh shell-client reload. |
| `qindaqt.launcher-controller` | Projection, query collapse, grant gating, activation + recent recording, denied-ancestor degraded truth with bounded diagnostics, null-collaborator fail-closed. |
| `qindaqt.launcher-composition` | Explicit XDG-root derivation, private-bus production policy composition, recording spawner/activator seams, denied-grant negative control, and no real application launch. |
| `qindaqt.launcher-offscreen` | Fatal-warning-clean compiled QML loading, QST provisioning, pinned/recent/category/search rendering, Tab and cross-section Up/Down traversal, Return/Space activation, Escape, persistence alerts, enabled/denied accessible states, and null-controller fallback. |
| `qindaqt.launcher-panel-dispatcher` | Fatal-warning-clean production-row controller injection plus preview null-controller dispatch. |
| `qindaqt.launcher-runtime-boundary` | Source policy: pure model platform-free; platform reach confined to adapter files; poison negative control. |
| `qindaqt.launcher-contract-text` | Mutation-sensitive launcher/applet-runtime/ADR and safety-comment truth for registry readiness, inert process fixtures, wholesale stored-list rejection, and ENOENT-only normal root absence. |
| `qindaqt.launcher-installed-package` | `LauncherAppletRuntime` relocates the shell, manifest/profile/policy/theme, compiled Launcher/Controls/Tokens closure, and warning-clean null-controller probe under source/build poison. |
| `qindaqt.shell-runtime-component-closure` | Independently installs the Launcher component and proves the staged shell resolves its Launcher/Controls/Tokens libraries without ambient loader state. |

The shipped Settings schema defines both launcher lists in the `panels` domain.
The former `shell.launcher.*` spellings were never admitted by the shipped
schema and caused the entire scoped snapshot to fail with `UnknownKey`.
[ADR-0076](../adr/0076-register-launcher-persistence-in-panel-settings.md)
records the corrected persistence contract. The schema-backed regression above
covers persisted pins and recents through real service-owner replacement and a
fresh shell client; it does not substitute same-owner epoch replacement for a
process restart.

## Non-claims

This slice proves no startup-notification activation tokens, no real application
session-bus activation, and no physical or nested-session behavior. Headless rows use `QCoreApplication`; all
launcher rows remove inherited display and session-bus endpoints, while the
two genuine GUI rows force offscreen software rendering. Tests use injected
roots and fakes. `/bin/true` and `/bin/false` are inert process-start fixtures only.

The browser uses `Popup.Window`: its focusable transient can extend outside
the layer-shell panel and Escape closes it. The compiled offscreen gate
checks that content belongs to a separate window and drives keyboard input
through that focused window. Configuration homes and authentication paths are
preserved exactly; loader injection and QindaQt development overrides remain
excluded from launched children.

Launcher result rows use Qt Quick Templates with token-owned backgrounds,
state overlays, focus rings and labels. Native application styles cannot
combine a light result background with dark-theme foreground tokens. The
compiled gate republishes dark, light and high-contrast themes on existing
rows and verifies their text contrast and rendered background pixels.

Startup and owner-loss availability notices clear as soon as a confirmed
Settings1 baseline restores persistence. Genuine save refusals and malformed
stored-list explanations remain visible until a subsequent explicit save;
connectivity recovery does not erase those outcomes.

When usable applications remain, skipped malformed desktop files do not show
an unqualified error above the browser. The catalog retains its degraded
inventory state and per-source diagnostics; enable
`QT_LOGGING_RULES="qindaqt.launcher.scan.debug=true"` to log those details once
per scan. If no applications can be loaded, the browser instead shows an
actionable catalog-level message. These presentation rules preserve valid
application launch behavior and never hide the detailed diagnostic data.
