# ADR-0074: Compose shell layout and token preferences through Settings1

- **Status:** Accepted
- **Date:** 2026-09-05
- **Owners:** Shell runtime
- **Supersedes:** None
- **Superseded by:** None

## Context

The 2026-09 project audit (A05–A07) established three integration defects: the
production shell ignored the confirmed `panels.layoutProfile` selection on
ordinary startup, its profile catalog lookup disagreed with the Settings
user-profile store, and `appearance.theme` plus the accessibility token
preferences never reached shell token publication. The Settings Customize route
had already promised that an applied profile is adopted at the next shell
start, and [ADR-0028](0028-compose-appearance-settings-through-settings1.md)
made Settings1 the composition boundary for appearance preferences.

The shell's settings client is asynchronous and surfaces are planned early, so
the confirmed selection must be known before the initial surface plan without
making startup hang when Settings1 is unavailable, and without reading the
service's private storage.

## Decision

- Before catalog selection, the shell performs one bounded (2 s) read of the
  scoped Settings1 snapshot covering `panels.layoutProfile`, `appearance.theme`,
  `fonts.family`, `fonts.pointSize`, and `accessibility.{highContrast,
  reducedMotion,reducedTransparency,textScale}` through the public
  SettingsClient transport. The shell never opens settings storage files.
- Startup selection precedence is, in order: an explicit `--profile`/`--theme`
  command-line value, the confirmed Settings1 preference, then the built-in
  fallback (`qindaqt` profile, profile `defaultTheme`). The option parser no
  longer injects a default profile id, so an unset flag stays distinguishable.
  A saved preference naming an uninstalled profile or theme recovers with a
  diagnostic — to the `qindaqt` profile or the selected profile's default theme
  respectively — so a deleted or renamed choice never strands login; only an
  explicit unknown `--profile`/`--theme` fails startup.
- Profile catalogs merge low-to-high precedence — source tree (only for the
  genuine build-tree executable), installed system roots, then the writable
  user store — with duplicate ids rejected within one directory and later
  directories overriding earlier ones. This matches the Settings Customize
  route's catalog contract. Explicit `--profile-dir`/`QINDAQT_PROFILE_DIR`
  remain isolated single-directory overrides.
- Confirmed appearance and accessibility preferences feed the shell token
  publisher at first publication and again on each later confirmed snapshot.
  `fonts.family` overlays the selected theme's family in the derived token set.
  Owner loss, bus failure, and malformed snapshots retain the last confirmed
  safe state; an explicit `--theme` locks theme selection for the process
  lifetime while accessibility and font preferences still apply. Layout profile
  changes are adopted only at the next shell start, as the Customize route
  promises.
- The same confirmed change reaches the raw `theme` maps consumed by panel and
  notification QML: the panel window factory and the notification window
  controller replace their cached map for future windows and push the new map
  onto every live window's `theme` property. Those QML properties are
  deliberately plain (non-readonly) so this update path works.
- The shell does not consume `appearance.wallpaper`, interface scale, or
  rendering preferences; those remain owned by their respective components and
  are out of this contract's scope.
- `--list` remains a deterministic inspection tool and never contacts
  Settings1.

## Consequences

- Ordinary restarts honor the saved layout and appearance choices; the audit's
  reproduction path (apply in Settings, restart without injected arguments) now
  works end to end, including recovery when the saved profile no longer exists.
- Startup gains a bounded blocking read; when Settings1 is absent the shell
  starts on built-in defaults after at most the deadline.
- A live confirmed `appearance.theme`/`fonts.family` change updates QST-1
  tokens and the raw panel/notification theme maps on existing and future
  surfaces in the same step; layout profile changes still require a restart.
- Focused tests cover the decode contract, selection precedence, catalog merge
  semantics, the save → restart read through a real Settings1 service on a
  private bus, real-binary startup exit codes for the recovered and explicit
  paths, live bridge retention across owner loss, and raw-map propagation to
  live windows.

## Revisit when

- The shell adopts live profile switching (removing the restart contract), or
- Settings1 offers a synchronous confirmed-read API, making the bounded wait
  unnecessary.

## Follow-up

[ADR-0078](0078-own-wallpaper-surfaces-in-the-shell.md) adds the production wallpaper consumer while retaining this decision’s Settings1 authority and snapshot rules.
