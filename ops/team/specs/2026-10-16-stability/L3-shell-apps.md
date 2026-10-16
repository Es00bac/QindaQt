# L3 — Shell, Settings and audio client (container-wm) — Sonnet

Base: container-wm hub `main` at the commit named in the manager's assignment. Branch `stability/l3-shell-20261016`.
Worktree: `~/work_SPaC3/container-wm.worktrees/stab-l3-shell-20261016`. Voice seat: `claude-helper-three`
(create the folder if missing).

## Why
- **Shell CPU.** The shell spends about 30 ms of CPU per task-list update, 10–14 times a second.
  - Icon lookup is uncached and probes the disk per row (`icon_theme_locator.cpp`).
  - Every row carries `generationRevision`, so the keyed row model sees every row as changed.
  - The command search, quick-launch and active-app controllers rebuild even while hidden.
- **Theme reset.** The saved theme is rejected at every login. `QStandardPaths::locate` takes only the first
  `qindaqt/themes` directory, and a stale `~/.local/share/qindaqt/themes` (19 symlinks) shadows
  `/usr/share/qindaqt/themes` (28 themes).
- **Settings.** It aborted 10 times, and one instance stopped draining its bus connection. That made the
  session bus hold about 4 GB of queued messages.
- **Audio Levels.** Every audio client subscribes to Levels at 20 Hz and decodes it even with no meter visible.
  OBS's client retries every 200 ms with no backoff.
- Findings items 1 (client side), 6 (shell side), 10, 12 (audio and OBS) and 15; optionally 17.

## Owned paths
- `src/shell/task_list/**`, `src/shell/icons/**`, `src/shell/desktop_controls/**`, `src/shell/launcher/**`,
  `src/shell/common/catalogpaths.*` and the shell runtime's theme-loading call site
- `src/apps/settings_center/**`
- `src/services/audio_client/**`, `src/services/audio_service/src/wireplumber_worker_metering.*`, `src/obs/**`
- optional (outcome 6): the shell QML popup files named below
- the tests and wiki pages for these

## Outcomes
1. **Task list rows.**
   - Exclude `generationRevision` (and any other per-snapshot field) from per-row change detection, so
     unchanged rows emit nothing and a title change emits one `dataChanged` for that row and role.
   - Cache icon resolution per (theme, icon name, size); invalidate on icon-theme change.
   - Command search, quick-launch and active-app controllers rebuild only when visible, or when their own
     inputs changed.
2. **Catalog directories.**
   - Theme, applet and applet-policy catalogs merge every XDG data directory. User entries override system
     entries by id, the way Settings already merges themes.
   - Fix the warning text at `shellruntimeapplication.cpp` around :268, which prints the requested theme while
     claiming it is the fallback.
   - Test with a user directory holding a stale subset.
3. **Settings shutdown.**
   - Destroy the QML engine and root objects before any model passed through `setInitialProperties`: declare
     the models before the engine, or delete the engine explicitly first.
   - `--help`, `--version` and other early exits must not call `exit()` with a live QApplication and QML
     state.
   - Verify with an offscreen run that opens and closes cleanly. Use AddressSanitizer if practical.
4. **Settings dispatch stall.** Find why a Settings process (Appearance page, open about 26 hours) stopped
   reading its session-bus socket while both its main and QDBusConnection threads sat idle in poll.
   - Suspects: blocking `QDBus::Block` calls or nested event loops inside D-Bus signal handlers or QML slots;
     libdbus's client receive limit tripping because dispatch stalls; `setDispatchEnabled` paths; a
     subscription to a 20 Hz broadcast never consumed.
   - Fix what you find, and make Settings subscribe only to signals it needs while their page is visible.
5. **Audio Levels on demand.**
   - The audio client adds the Levels match rule only while at least one consumer holds a reference, and
     drops it when the count reaches zero. The shell and Settings acquire that reference only while a meter is
     visible.
   - If the service can cheaply stop metering when nobody listens, do it without changing the public
     contract. Otherwise document why not.
   - The OBS audio client backs off exponentially, for example 0.2 s doubling to 10 s, after rejected
     snapshots.
6. *(Optional, if time allows)* **Fewer GL contexts.** The shell keeps one window, and therefore one GL
   context, per popup ever opened: a tooltip and context menu for each dock and task tile, and each applet's
   `PanelPopup`. Share one tooltip and one context menu per strip, and create rarely used popups through a
   Loader that is released on close.

## Acceptance
- Unit tests for:
  - row diffing (a title change touches exactly one row and role);
  - icon cache hits and invalidation;
  - catalog merge precedence;
  - audio subscription refcount (match rule present only while referenced);
  - OBS backoff timing with an injectable clock.
- An offscreen Settings start/close loop (for example 20 runs) with zero aborts.
- Focused build and ctest. Update the wiki pages (task list applet, theme catalog, audio client, Settings),
  then run strict mkdocs and the link checker.

## Reference designs (study, don't depend)
- KDE libtaskmanager: role-scoped `dataChanged`.
- GTK and Qt icon-theme caches.
- gnome-control-center: panels connect to services only while visible.
