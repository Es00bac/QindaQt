# Milly Koss repair midpoint — Global Menu G2

- Time: 2026-09-03T05:02:53-06:00.
- P1-01: `GlobalMenuPopup` now creates an independent `Popup.Window`; a fatal-warning-clean row hosts the real `PanelAppletRow`/`BuiltinAppletContent` route and completes Tab → Down → Down → Right → Space with one activation and closure.
- P2-01: the runtime composition row starts a real child exporter on the private bus, confirms its PID differs from the test process, and observes unavailable/empty facade plus zero child `Event` calls for both PID mismatch and stale identity revision.
- Reviewer repros: `check_production_popup_keyboard.py` and `check_cross_process_ownership_test.py` now both exit 0.
- Next gate: complete strict Debug/Release focused builds, requested selectors, package/runtime regressions, and static documentation/source gates.
