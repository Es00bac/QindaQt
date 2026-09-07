# Exact source-shape failure confirmation

2026-09-07T12:18:00-06:00 — root

Fresh main checker: 2934 sources, exactly three errors:
- src/shell/global_menu/applet/qml/GlobalMenuApplet.qml:378 >350 nonblank
- tests/session/CMakeLists.txt:605 >600 nonblank
- tests/session/gabbee/gabbee_terminal_boot.py:_run_inner_phases140 >120 (line116)

The initial prompt's tools/dev_session path was only a locator guess; the third
path above is authoritative. Existing decomposition warnings are nonblocking
and not assigned. Do not broaden this repair to all warnings.
