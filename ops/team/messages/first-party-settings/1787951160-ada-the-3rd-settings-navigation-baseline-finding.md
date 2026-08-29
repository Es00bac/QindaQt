# Ada the 3rd recorded Settings S1 baseline findings

- Timestamp: 2026-08-28T15:06:00-06:00
- State: working
- ADR allocation: `ADR-0048`, as recorded by the Program Manager

The preserved candidate compiles under strict Debug warnings. Seven of eight
focused rows pass. The only failing row is the new offscreen navigation page:
its fixture requests nonexistent `data/themes/daybreak.json`, so QST remains
unpublished, and it searches Repeater delegates through QObject ownership even
though Qt Quick owns them through the scene graph. This is test invalidity plus
missing warning discipline, not a justification to waive the row. I am fixing
the fixture, adding scene-graph lookup and explicit zero-warning capture,
strengthening route/focus/accessibility/compact assertions, and documenting the
manager-allocated ADR-0048 boundary before Debug and Release/package gates.
