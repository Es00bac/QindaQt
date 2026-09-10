# Drop-targeting fix installed via Portage `-r1` (kimi, user-directed)

Follow-up to `2026-09-09T191622-0600-kimi-drop-targeting-fix-and-snapshot.md`.

## Outcome

`emerge -1 =gui-wm/qindaqt-desktop-0.1.0_pre20260909-r1` completed
(exit 0). `/usr/lib64/qt6/plugins/kwin/plugins/qindaqt_compositor.so` and the
rest of the desktop image were installed at 19:52 from snapshot `e0c7ff78`.
The running KWin session still has the previous plugin loaded; the fix takes
effect on next login (or compositor restart), consistent with prior installs.

## Correction to the snapshot notice

The calendar failure under Portage was not the predicted `-Werror`
unused-parameter. The pinned calendar sources do not compile at all:
`src/apps/calendar/settings/calendar_preferences.cpp` calls
`QStringLiteral()` on non-literal arguments (first at line 23, more around
lines 46-47). All 15 compile errors in the failed build log were confined to
`apps/calendar`. Nothing else links against the calendar plugin (runtime
load), so the ebuild now applies FILESDIR patch
`qindaqt-desktop-0.1.0_pre20260909-r1-skip-inflight-calendar.patch`, which
removes only the two `add_subdirectory(apps/calendar)` lines at prepare time.
The calendar lane owns the repair; when a compiling candidate lands, drop the
patch in the next ebuild revision. No calendar sources were modified by this
workstream.

## State

- Repo ebuild + patch committed with the doc checkpoint update
  (`docs/wiki/development/gentoo-desktop.md` now names `-r1` / `e0c7ff78`).
- Overlay copy and Manifest regenerated; accept_keywords entry unchanged.
- Nested live qualification of the late-Shift gesture remains outstanding.
