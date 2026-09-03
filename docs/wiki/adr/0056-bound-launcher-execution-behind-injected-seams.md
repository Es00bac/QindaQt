# ADR-0056: Bound launcher execution behind injected seams

- **Status:** Accepted
- **Date:** 2026-09-02
- **Owners:** Shell launcher lane
- **Supersedes:** None
- **Superseded by:** None

## Context

[ADR-0042](0042-launcher-model-without-execution.md) kept the launcher L0
model pure: no filesystem, no persistence, and launch requests that carry no
command line. Launcher L1 must now scan real installed applications, persist
pinned/recent identities, and start processes — without letting hostile
desktop-entry content reach execution logic and without moving any of that
reach into the pure model.

Three execution surfaces exist in the desktop-entry specification: plain
`Exec` argv, `Terminal=true` entries that need a terminal, and
`DBusActivatable=true` entries that are activated over the session bus
instead of spawned. A single "run a command" path would silently
mis-execute two of them.

## Decision

Launcher L1 adds production adapters in `src/shell/launcher`, each behind its
own seam, leaving the L0 model untouched:

- **Scanning** (`ApplicationScanner`) reads only the `applications/` trees of
  caller-injected roots; the composition root resolves the XDG
  data-home/data-dirs list and nothing reads the environment implicitly.
  Canonical containment rejects links that escape an injected root, only
  regular files are opened, and capped reads enforce the byte ceiling even if
  a file grows after metadata inspection. File count and decode ceilings apply;
  rebuilds publish a
  monotonically increasing generation so consumers fence stale reactions;
  a debounced `QFileSystemWatcher` drives refresh. Unreadable roots, files,
  and oversized documents are degraded truth with diagnostics, never fatal.
- **Execution planning** re-extracts only the four execution keys (`Exec`,
  `Terminal`, `Path`, `DBusActivatable`) from the raw document the scanner
  already validated and retained. The pure model still never carries a
  command line; ADR-0042's boundary stands. Every launch resolves through
  the catalog's single `makeLaunchIntent` resolver first — an entry the
  catalog does not publish (unknown, hidden, shadowed) is refused before
  any execution planning.
- **Process start** goes through an injected `LaunchSpawner` seam. The
  production spawner uses `QProcess::startDetached` with an allowlist-sanitized
  child environment and the entry's `Path`; there is no shell interpolation
  anywhere. Field-code expansion drops the file/URL and deprecated codes as
  whole tokens and refuses embedded or unknown codes rather than guessing.
- **Terminal=true** entries route through an injected terminal command prefix
  (the composition root wires the QindaQt Terminal launch policy) or are
  refused truthfully when no policy is wired. There is never a silent shell
  fallback.
- **Desktop actions** contribute only action-group `Exec`; entry-level
  `Terminal`, `Path`, and `DBusActivatable` govern every action. Lookalike
  policy keys inside action groups are ignored without decoding.
- **DBusActivatable=true** entries are activated through
  `org.freedesktop.Application.Activate` / `ActivateAction` on the session bus
  behind an injected `LaunchActivator` seam. Dispatch and completion are
  separate truths. Startup-notification activation tokens are a later slice;
  the platform-data map is empty today.
- **Pinned/recent persistence** consumes the public Settings1 client under the
  key set `shell.launcher.pinned` / `shell.launcher.recent`, storing bounded
  string lists of desktop-entry ids and nothing else, with ADR-0012
  draft/apply/no-replay semantics and fail-closed behavior on transport loss.
  Registering the key set in the Settings1 schema is a settings-schema
  authority change owned outside this lane; until then a production service
  answers `UnknownKey` and the controller reports the save as refused.
- The applet registers in the compiled first-party registry as
  `qindaqt.applets.launcher` with the `applications.launch` grant through the
  audited manifest/policy path. Hosting the applet in the production panel
  QML composition is a later lane's slice.

## Consequences

- Hostile desktop entries can degrade the surface but cannot inject shell
  syntax, exceed execution bounds, or reach a process without catalog
  membership.
- Tests never start real applications: the spawner and activator are
  interfaces, and the only real children any test starts are the inert
  `/bin/true` and `/bin/false` fixture executables.
- The launcher's environment allowlist (session identity, locale, display,
  runtime discovery, `LC_*`) is a single documented authority; anything
  unlisted, including QindaQt's own development overrides, does not
  propagate to children.
- Pinned/recent truth survives restarts only once the Settings1 schema owns
  the launcher keys; until then the UI shows last-confirmed values and
  honest save-refusal truth.
- The production panel still renders no launcher content until the hosting
  lane wires the dispatcher; the registry entry asserts only that the
  compiled implementation exists in this build.

## Revisit when

Startup-notification token support lands, a third-party launcher package
needs the mediated launch interface, the Settings1 schema registers the
launcher keys, or the panel hosting lane needs activation actions beyond
`Activate`/`ActivateAction`.
