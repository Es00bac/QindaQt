# ADR-0220: Rebuild the System Monitor on QindaTK as a dock dashboard

- **Status:** Accepted
- **Date:** 2026-09-19
- **Owners:** Bundled applications

## Context

The System Monitor was never merged. It sat on
`worker/system-monitor-integration`, which fell 470 commits behind main, and
the packaged build stopped starting altogether: it exits 3 on
`qinda-dark.json` because it predates the `glyph` button style that
`theme_loader.cpp` accepts today. Measured against btop — the tool people
actually reach for — the application it would have shipped was also weaker
than the alternative, and the reason was structural rather than cosmetic.

btop's value is simultaneity. Processor, memory, storage, network and
processes answer each other: a saturated device and a writing process are one
observation. The monitor presented seven views in a `QStackedWidget`, which
can show exactly one of them at a time, and no amount of restyling recovers
what tabs throw away. Its plots were small `QWidget`s with no shared notion of
scale or colour, so nothing could be compared across panels either.

QindaTK exists for dense, panel-heavy desktop UIs at 10–12px and already
carries a dock model with lanes, tab groups, floating panels and persistence.
What it did not have was any way to plot a value over time, which is the one
thing a system monitor is made of.

## Decision

Keep the sampling core; replace the interface.

`procfs_reader`, `sample_collector`, `process_model`, `process_actions`,
`monitor_engine` and the hardware samplers are carried over unchanged in
substance, and the ADR-0108 boundary is unchanged with them: the interface
consumes published snapshots and never reads procfs. Only the QtWidgets UI and
its AppShell bridge are dropped. `ProcessCounters` gains `ppid`, without which
a process tree has no parent and every row is a root.

The interface is QML on QindaTK, arranged as one dense dashboard inside a
`Tk.DockHost`. The process table is the host's canvas, because a canvas is the
one region that cannot be torn off or hidden and the process list is the panel
this application would be pointless without. The arrangement persists under a
storage key, so a layout someone settles on is the one they get back.

The detachable-view contract survives in a stronger form. A detached view is no
longer a second page of the same stack but a second *process*, started as
`qindaqt-system-monitor --panel <id>`: the windows then sample independently,
and a wedged sysfs read or a crash in one cannot take the others down.

Two view-model pieces sit between the engine and QML. `ProcessTableModel`
flattens filtering, ordering and tree assembly into one list with an O(1)
`get()`; it is deliberately not a `QSortFilterProxyModel`, because tree
assembly is not a filter — a surviving child must be placed under a parent the
filter removed, with a stable depth. `MonitorFacade` samples hardware on its
own slower cadence and owns the formatting rule that an unavailable reading is
a dash and never a zero.

The toolkit additions this required — `Tk.Graph`, `Tk.GraphSeries`,
`Tk.Meter`, `Tk.Sparkline`, `Tk.DataTable`, `Tk.TableColumn` and
`Theme.ramp` — are additive in QindaTK and documented there; see QindaTK
decision D-014 for why `Graph` carries two drawing paths.

The menu is AppShell's, not QML's. One `ActionSpec` catalog feeds
`ApplicationCoordinator`, and `composeFirstPartyMenuExport` publishes it as a
`com.canonical.dbusmenu` endpoint for the shell's global menu, writing
`inWindowMenuVisible` on the root window so the in-window bar disappears
exactly when the desktop takes the menu over. Shortcuts are instantiated from
the same catalog, so they keep working when this window draws no menu at all.

## Consequences

The application now depends on `dev-libs/qindatk` at runtime. It is a QML
application, so nothing links the toolkit's C++ API; the module has to be on
Qt's QML import path, and a build tree needs `QML_IMPORT_PATH` pointed at it
when testing against an uninstalled toolkit.

The view model is a library (`QindaQt::SystemMonitorView`) rather than part of
the executable, so it can be tested without a QML engine or a window.

`--grab <file.png>` renders one frame offscreen and exits. Without it the only
way to review this interface is a live session, which no test and no automated
reviewer can rely on having; it is the same affordance `qtk-preview --grab`
provides for the toolkit.

Every action must carry a shortcut: `ActionRegistry` rejects one that does not
("invalid presentation state") and the rejection is atomic, so a single bare
entry costs the application its whole menu — in the panel and in the window
both. `tst_system_monitor_actions` holds that line.

Readings are coloured by magnitude through the shared theme ramps, so panels
agree with each other and with the numbers printed beside them. Changing what
a colour means is now a theme change, not an edit in six panels.
