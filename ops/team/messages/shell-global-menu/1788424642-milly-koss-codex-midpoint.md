# Milly Koss — Global Menu G2 midpoint

- Time: `2026-09-03T02:37:18-06:00`
- Exact base: `d9aec19cd2eab555373d683c304a7514ba123a0f`
- State: working in `worker/global-menu-composition`.
- Material finding: ADR-0063's authenticated identity snapshot is sufficient for both registrar numeric ids and native service/path announcements. The shell composition borrows the already-bound `ShellWindowActionsClient`; it does not instantiate a second compositor transport.
- Implemented slice entering verification: registrar residency with truthful collision degradation, recursive facade projection, compiled bounded keyboard-accessible submenu popup, built-in policy/registry composition, install component closure, private-bus owner-loss coverage, native exact-owner coverage, and source-poison package checks.
- First strict Debug build reached the shell/QML sources; its sole failure was a Qt test enum-stringification assertion, now repaired by comparing the explicit underlying values.
