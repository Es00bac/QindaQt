# Claim: finish-shell-launch-gates test-environment repairs

2026-09-06T10:57:00-06:00 — finish-shell-launch-gates-kimi

Claimed the bounded two-gap repair on isolated branch `fix/finish-shell-launch-gates`
at exact base `747108d2` (worktree `.cache/finish-shell-launch-gates`).

Owned paths: `tests/shell/tst_shellruntime_startuplaunch.cpp`,
`tests/shell/audio_applet/run_shell_component_closure.cmake`, plus focused
helper/docs only if essential. No production changes unless a concrete product
defect is proven.

Findings so far (from source inspection, pre-build):

1. `absentServiceUsesBuiltInDefaults` starts dbus-daemon with `--session`, which
   loads the host session configuration with standard service directories. A
   host-installed Settings1 provider can then be autoactivated and answer the
   shell's snapshot read, so the expected "no confirmed Settings1 preferences"
   warning never prints. The already-integrated fix in
   `tests/shell/tst_shellstartuppreferences.cpp` (explicit private bus config,
   no servicedirs, scope-guard cleanup) is the pattern to mirror.
2. `run_shell_component_closure.cmake` unsets DISPLAY/WAYLAND_DISPLAY and runs
   the staged `qindaqt-shell --help`. Production `src/shell/runtime/main.cpp:9`
   constructs QGuiApplication before `parseRuntimeOptions` processes `--help`,
   so even help needs an initializable platform plugin. With no display and no
   QT_QPA_PLATFORM the process aborts. The fixture (not production) is wrong;
   the closure script must supply `QT_QPA_PLATFORM=offscreen`, matching how the
   repo runs the shell for headless inspection (`qindaqt.shell-runtime-catalog`).

Next: apply both fixture repairs, private build with <=2 jobs and disk TMPDIR,
run the two focused ctest gates, then hand off one exact candidate commit.
