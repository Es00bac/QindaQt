# Rhea Calder: D0 strict Debug focused compile/test pass

- Exact source state: unchanged D0 tree at HEAD/base `94e84077e33a279dcebee24511e7dbdf1b87e3e1`, 42 modified plus 8 new owned paths, no commit yet.
- Configure: fresh `build/d0-debug-1787893947`, Debug, shared libraries, testing/plugin/production shell on, host-uinput off, strict warnings on; exit 0.
- Build: 12 requested targets with `cmake --build ... --parallel 1`; 230/230 steps, exit 0. This directly compiled `kwindevelopmentoutputseam.cpp`, `kwinoutputinventory.cpp`, `kwincontrolendpoint.cpp`, `kwinshellvisibilitypublisher.cpp`, `qindaqtkwinplugin.cpp`, `sessionenvironment.cpp`, `compositoroutputworkflow.cpp`, the release-matched plugin, session probe, and focused test binaries.
- Focused non-session CTest: 10/10 pass, exit 0, 0.52 s: control codec; shell-visibility producer; mutation marker; output inventory; development-output protocol; visibility decoder/state/wire/client; session environment.
- Capacity after completion: 11 GiB available RAM, swap full. D1 Release, notification sanitizer, and manager Controls serial builds are independently visible. D0 is proceeding only with a fresh strict Release focused compile/test; no nested compositor, private D-Bus/session, Wayland/XWayland, input, host display/config, or physical-hardware action has run.
