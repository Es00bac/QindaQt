# ADR-0209: `windowManagement.*` is bridged into kwinrc by the session, live

- **Status:** Proposed
- **Date:** 2026-09-18
- **Owners:** Session, Compositor
- **Supersedes:** None
- **Superseded by:** None

## Context

Settings1 has carried five `windowManagement.*` keys since schema v2
(`dockingModifier`, `snapDistance`, `focusPolicy`, `sessionRestore`,
`closeContainerPolicy`). They validated, persisted and round-tripped, and
changed nothing: no process read them. The behaviours they name live in two
places that never see Settings1. KWin owns focus policy and snap zones and
reads them from `kwinrc` on `org.kde.KWin.reconfigure`. The QindaQt
compositor plugin owns the docking chord (`InteractionController`'s exact
pointer bindings, mirrored by the late-takeover detector) and the container
close prompt, and until now both were compile-time constants.

`sessiondefaults.cpp` already writes kwinrc, but as first-run seeds that a
user's own choice always wins over; it runs once at launch and must stay
that way. A live setting needs a resident writer that runs for the whole
session and a consumer that re-reads on the compositor's own reload signal.

## Decision

1. **The session process hosts the bridge.** `qindaqt-session` owns a
   purpose-scoped `SettingsClient` over exactly the five keys
   (`src/session/window_management/`). Every confirmed snapshot is decoded
   totally: a value outside the schema fails the whole snapshot and the last
   good preferences stay in force. Decoded preferences are written with
   KConfig into the user's `kwinrc` (`SimpleConfig`, so no cascaded defaults
   are copied in and every foreign group survives), and only when at least
   one entry actually differs is one `org.kde.KWin.reconfigure` requested,
   debounced so a burst of edits costs one reload.
2. **The kwinrc mapping is fixed.** KWin-native knobs go where KWin reads
   them: `[Windows] FocusPolicy` (`ClickToFocus` / `FocusFollowsMouse` /
   `FocusUnderMouse`) and `BorderSnapZone` = `WindowSnapZone` =
   `snapDistance`. QindaQt-owned knobs go to a `[QindaQt]` group in the
   Settings1 spellings: `DockingModifier`, `CloseContainerPolicy`,
   `SessionRestore`.
3. **The compositor plugin consumes `[QindaQt]` on `Options::configChanged`.**
   `KWinHybridSession` re-reads the group when KWin reconfigures and applies
   it live. `DockingModifier` rebinds the exact pointer chord in both places
   that judge it, the controller and the late-Shift takeover detector;
   `Shift` stays in every chord (`super` = Meta+Shift, `alt` = Alt+Shift,
   `control` = Ctrl+Shift) because a bare modifier plus left button is
   KWin's own move command, and `disabled` makes no press match at all.
   `CloseContainerPolicy` other than `ask` bypasses the close prompt and
   applies the standing decision. `SessionRestore` is written and read but
   has no consumer yet; it is recorded as such rather than invented.
4. **Seeds remain seeds.** `sessiondefaults.cpp` is untouched; the bridge is
   a separate module with its own tests and never runs at first launch
   before Settings1 is up.

## Consequences

- The Windows & workspaces route can now be built on the five keys and every
  edit takes effect in the running session without a restart.
- A kwinrc edited by hand still works: the bridge only writes when Settings1
  confirms a change, and the compositor reads whatever the file holds.
- Proof: `qindaqt.window-management-preferences`,
  `qindaqt.kwin-window-management-writer`, `qindaqt.window-management-bridge`
  (real resident service on a private bus, real KConfig writer, a fake
  `org.kde.KWin` counting reconfigures), `compositor.window-management-config`,
  the rebind cases in `hybrid.interaction-controller` and
  `hybrid.late-shift-takeover-detector`, and the nested row
  `compositor.window-management-bridge.docking-chord.single-1080p`, which
  rewrites the private kwinrc, calls the real `reconfigure`, and proves the
  old chord goes inert, the new one docks, `disabled` docks with nothing,
  focus-follows-mouse activates by hover, and the restored chord docks again.
- Open: a consumer for `sessionRestore`, and a nested proof of the close
  policy (its decoding is unit-covered; the prompt bypass is exercised only
  through the shell's close action).
