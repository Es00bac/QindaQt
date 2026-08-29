# Dijkstra the 2nd — Terminal real-adapter compile blocker

- Time: 2026-08-28T17:09:10Z
- Exact candidate: `9bd54448b5735b540bfef8ccbf54ffe1dee0e88b`
- Severity: P1 (production Terminal cannot build)
- Current counts: P0/P1/P2/P3 = `0/3/3/4`
- Product edits: none

The fresh root configure succeeded with strict warnings, Debug, tests enabled,
shell/production shell/KWin plugin/host-uinput disabled, and only an extracted
repository package `qtermwidget 2.4.0-1` added to `CMAKE_PREFIX_PATH`:

```text
cmake -S . -B /tmp/dijkstra-terminal-gate.8l5adE/build -G Ninja
  -DBUILD_TESTING=ON -DQINDAQT_BUILD_SHELL=OFF
  -DQINDAQT_BUILD_PRODUCTION_SHELL=OFF
  -DQINDAQT_BUILD_KWIN_PLUGIN=OFF
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF
  -DQINDAQT_ENABLE_STRICT_WARNINGS=ON -DCMAKE_BUILD_TYPE=Debug
  -DCMAKE_PREFIX_PATH=/tmp/dijkstra-terminal-gate.8l5adE/prefix/usr
# exit 0
```

The serial build of `qindaqt-terminal` plus the five C++ Terminal tests stops
at adapter AUTOMOC compilation with exit 1:

```text
terminal_widget_adapter.h:46:61: error: cannot convert ‘QTermWidget*’ to
‘QWidget*’ in return
terminal_widget_adapter.h:12:7: note: class type ‘QTermWidget’ is incomplete
```

The header forward-declares `class QTermWidget;` at line 12, stores
`QTermWidget *m_widget`, and defines the virtual `QWidget *terminalWidget()`
inline at line 46. A translation unit including the header without
`qtermwidget.h`—exactly the generated MOC unit—cannot know that `QTermWidget`
derives from `QWidget`, so the standard derived-to-base conversion is
ill-formed. This is not a dependency-install issue; the audited package was
found and its include path appears in the compiler invocation.

Move the override out of line into the adapter `.cpp` after the qtermwidget
definition is included (or otherwise make the inheritance complete without
leaking the private header). Add the production adapter/executable target to
the mandatory strict compiled gate. The prior qtermwidget-free scratch build
could not discover this failure. The adapter-free support targets and safe
headless registered rows continue in the still-claimed serial lane.
