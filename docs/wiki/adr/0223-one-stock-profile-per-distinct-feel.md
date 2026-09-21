# ADR-0223: One stock profile per distinct feel

- **Status:** Accepted
- **Date:** 2026-09-20
- **Owners:** Shell presentation
- **Amended by:** [ADR-0224](0224-name-the-presentations-a-layout-asks-for.md), which creates the three missing applets and restores the centred Windows preset

## Context

Eleven stock layout profiles shipped, and most of them were not layouts so much
as reshuffles of one bar. Three were Windows-shaped (`qinda-bliss`,
`windows-classic`, `windows-modern`). Several placed applets with no `zone` at
all, so everything fell into one default zone in declaration order; that is how
`gnome-inspired`, `minimal` and `nextstep-inspired` all ended with the
`launcher` appended after the clock and the tray, and how `xfce-inspired` ended
up with two `quick-launch` instances.

Three profiles named plugins that have no manifest — `application-launcher` and
`grouped-task-list` in `unity-inspired`, `centered-task-list` in
`windows-modern` — so those instances silently resolved to nothing and the
layout showed a duplicated or missing control. `macos-inspired` and `qindaqt`
carried a `bare: true` setting on every applet that nothing in the runtime
reads. Panel thickness ignored the applets' own declared minimum cross extent:
`minimal` asked a 32-pixel `launcher` to live in a 26-pixel strip, and
`xfce-inspired` in a 28-pixel one.

The operator's judgement was that the Bliss taskbar is the quality bar and the
rest were "aligned and arranged in lazy, ugly ways", and asked for one distinct
feel per layout plus a genuinely QindaQt one. The existing `qindaqt` profile was
a GNOME top bar above a macOS dock — a composite of two other entries in the
same list, not a signature.

## Decision

Ship **one stock profile per distinct feel**: eight, not eleven.

`windows-classic`, `windows-modern` and `mate-inspired` are retired.
`qinda-bliss` is the Windows feel and is left exactly as it is, because it is
the reference. `minimal`, `macos-inspired`, `gnome-inspired`,
`unity-inspired`, `xfce-inspired` and `nextstep-inspired` are redesigned, and
`qindaqt` is rebuilt as the signature layout.

Each profile differs on the axes a user actually sees — edge, thickness,
alignment, length, layer, hide mode, and default theme — and each pairs with a
theme no other stock profile uses. Every applet instance carries an explicit
`zone`, and a profile declares only settings the runtime honours (`zone`,
`dockMode`, `presentation`, `grouping`, and the desktop-icons keys), so a stock
layout never ships a value that reads as configuration and does nothing.
Panel thickness respects each hosted applet's declared minimum cross extent.

The signature `qindaqt` layout gives three edges three jobs, which is what no
imitation in the set does:

- a 26-pixel top **ledger** carrying only the focused window's identity, its
  global menu, and the clock — thin because it hosts no launcher;
- a 52-pixel left **smart shelf** carrying the launcher, pins, and the task
  list vertically, because QindaQt's window containers and rolled-up cards
  need a column where long titles fit; and
- a 34-pixel bottom-right **instrument strip** on the `overlay` layer with
  `dodge-active`, carrying the dashboard and the first-party hardware chips
  (audio, Bluetooth, power, smart lights, OBS) plus the tray and utilities. It
  floats over windows and ducks the active one instead of reserving a band
  across the whole bottom edge.

The panel ids `command-bar` and `smart-shelf` are kept even though the shelf
moved from the bottom edge to the left, because recorded session evidence and
the shell-polish validator address the shelf by id; `isDockPanel` requires a
centred bottom panel, so a left-edge `smart-shelf` correctly stops being a
dock.

## Consequences

Every stock-profile invariant still holds for all eight: exactly one resolved
menu applet, one notification center, one status notifier, one clipboard in the
notification center's own zone (zero for Bliss), a task list exactly where the
workflow exposes tasks, a global menu only on a top panel and only in the three
families that declare one, and all thirteen desktop-control plugins placed by
at least one profile. `places-menu` moved to XFCE, where a Places menu belongs,
and `dashboard` to the QindaQt instrument strip; both would otherwise have lost
their only home when `mate-inspired` and `windows-modern` retired.

Retiring three profiles is visible to anyone whose `panels.layoutProfile`
setting names one: startup selection falls back the same way it does for any
unknown id. Derived user profiles are unaffected — they are separate files.

The capture matrix, the global-menu composition matrix, the applet-instance
resolver count, and the two output scenarios that named retired profiles were
updated. The 150 % and dual-output scenarios now use `nextstep-inspired` and
`qinda-bliss`, so the five S3 rows between them cover a top bar, a
top-plus-bottom pair, a left rail, a right-edge column, and a single bottom
taskbar.

`docs/wiki/handbook/catalog/assets.md` still records eleven profiles: it is an
explicit snapshot at a named commit, not a live mirror, and is left as written.
