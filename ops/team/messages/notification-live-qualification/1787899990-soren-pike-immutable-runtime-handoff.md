# Soren Pike — immutable Notification Live candidate handoff

Timestamp: 2026-08-28T06:53:10Z

## Exact candidate

- Commit: `557260a50faaf083733afe5972ad6541ef398108`
- Tree: `8f9f131461157b33bb88e0b4a46811e2308c9329`
- Parent and assigned base: `c4982697858c083828bd406f1aa56c4e942bcc10`
- Branch: `worker/notification-live`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/notification-live`
- Paths: 74; sorted path-manifest SHA-256
  `3be3d516f941c62d0d8f227258d0669fe71e336d787af9e7da3435755a98e731`
- Commit summary: 5,825 insertions, 224 deletions.
- Deliberately excluded: untracked `.omc/` state and all team-board files.

## Delivered outcome

The candidate stages and boots the production shell, resident notification
host, Settings1 service/application, supervisor, compositor launcher, and KWin
plugin inside a disposable private D-Bus/XDG/Wayland session. It proves real
nested compositor geometry, Meta+N dispatch, focus/keyboard traversal, Escape,
DND persistence/failure/recovery, critical bypass, settings launch containment,
shortcut disable/remap/restore, lock privacy denial, one bounded shell restart,
fresh shell authentication, resident-host continuity, baseline without replay,
and owner-authenticated record cleanup.

Runtime repair also removes an unexported KWin `LayerShellV1Window` dependency,
uses the public layer-surface/window boundary, fixes deferred center focus and
property writes, contains the staged settings child, and gives the restart
record one persistent private sender so the production owner-only close policy
is exercised rather than bypassed.

## Exact acceptance evidence

- Release `shell.notification-live.1080p`: PASS 1/1, 9.74s.
- Release WUXGA/1440p/125%/150%: PASS 4/4, 38.33s; 150% was 9.54s.
- Release `shell.notification-live.race-10x`: PASS 1/1, 94.69s, containing ten
  fresh complete inner repetitions.
- Post-modular-extraction `session.python-syntax` plus 1080p: PASS 2/2; 1080p
  9.86s.
- Fresh Debug production shell/plugin/probe build: PASS, 55/55 final build
  steps after regeneration.
- Release focused regression/integration gate: PASS 13/13, including public
  input protocol/injector, live-driver unit, Python syntax, nested/staged plugin
  discovery, supervisor, settings bridge, and notification QML/entry rows.
- Debug focused regression gate: PASS 11/11.
- Notification Python unit: PASS 10/10.
- QML lint: PASS.
- Public-ABI scan: PASS; staged plugin has no `LayerShellV1Window` symbol.
- Documentation validator: PASS, 44 Markdown documents plus navigation.
- Source shape: PASS, 800 files, zero skips/warnings/errors.
- Strict MkDocs: PASS using the pinned candidate venv.
- `git diff --cached --check`: PASS before commit.
- Final teardown: no matching nested QindaQt/KWin PID and no
  `/tmp/qindaqt-notification-live-*` root.

All live rows explicitly removed inherited `DISPLAY`, `WAYLAND_DISPLAY`, and
`DBUS_SESSION_BUS_ADDRESS`. No host bus, display, cursor, input seat, shortcut,
locker, configuration, password, or physical device was used.

## Remaining bounded caveats

This milestone does not claim screenshot visual baselines, a screen-reader/
full accessibility tree, physical input or GPU behavior, host-session locking,
multi-seat/session switching, alternative lockers, suspend/resume, or physical
mixed-output behavior. These boundaries are stated in the wiki.

## Requested next action

Assign a different worker to review the exact immutable commit above. If that
review passes, the manager should integrate it and rerun the affected gates on
the combined tree. No one else should edit or integrate this branch. Soren is
stopped and the sole private-runtime lane is released.
