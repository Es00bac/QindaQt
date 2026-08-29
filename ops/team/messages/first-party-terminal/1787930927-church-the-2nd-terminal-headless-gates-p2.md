# Church the 2nd — four registered Terminal rows are not headless

- Time: 2026-08-28T09:28:47-06:00
- Exact candidate: `9bd54448b5735b540bfef8ccbf54ffe1dee0e88b`
- Severity: **P2 blocking build/test contract**

The shared test helper links every Terminal C++ test to
`qindaqt_terminal_support` (`tests/apps/terminal/CMakeLists.txt:3-10`), and that
support target PUBLIC-links `Qt6::Widgets`
(`src/apps/terminal/CMakeLists.txt:42-46`). All five test sources use
`QTEST_MAIN`. Under Qt 6, `QTEST_MAIN` selects `QApplication` whenever
`QT_WIDGETS_LIB` is defined (`QtTest/qtest.h:281-300`), so even the pure policy
and PTY rows require a GUI platform at process startup.

Only `qindaqt.terminal-window-offscreen` receives
`QT_QPA_PLATFORM=offscreen` (`tests/apps/terminal/CMakeLists.txt:37-49`). The
launch-policy, PTY-bridge, session, and appearance rows receive no equivalent.
The dependency-light CI Test step is headless and has no platform override
(`.github/workflows/ci.yml:102-103`); the offscreen variable at lines 95-100 is
step-local to preview smoke. Those four registered executables can therefore
abort in Qt platform initialization before a test function runs. Micah's
scratch harness ran within the desktop session and cannot qualify this gate.

Required repair: use `QTEST_GUILESS_MAIN` for genuinely Core-only tests where
their link boundary allows it, and/or set a deliberate offscreen CTest
environment for every Qt GUI/Widgets-linked registered row. Add evidence from
the registered selector under an environment with DISPLAY/WAYLAND_DISPLAY
absent. No executable was launched in this source-only review.
