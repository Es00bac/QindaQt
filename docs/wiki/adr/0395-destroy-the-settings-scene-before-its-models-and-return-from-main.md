# ADR-0395: Destroy the Settings scene before its models and return from main on early exits

- Status: Accepted
- Date: 2026-10-16

## Context

`qindaqt-settings` ended in SIGABRT twelve times between 2026-09-28 and
2026-10-16, one of them from `qindaqt-settings --help`. Two causes were visible
in the source:

- `main()` declared the `QQmlApplicationEngine` before every model it hands to
  QML with `setInitialProperties` (the engine has to come first: the token
  facade, the icon runtime and the appearance model live in it). Scope exit
  therefore destroyed the models first and the QML root last, so every binding,
  `Connections` and `Component.onDestruction` handler ran against an object that
  was gone. The route-construction probe carried a workaround for this (it
  deleted the root objects itself before quitting, because ordinary teardown
  "emits binding warnings"), which also meant no test ever exercised the real
  shutdown.
- `QCommandLineParser::process()` calls `exit()` for `--help` and for a usage
  error, while the `QApplication` is alive and static destruction runs around
  it.

## Decision

- `QmlTeardown` (`src/apps/settings_center/settings_qml_teardown.h`) is declared
  in `main()` after the last model and before the scene is loaded. Reverse
  declaration order destroys it first on every exit path (a plain return, the
  route probe, an exception). It deletes the engine's root objects and collects
  the script heap before any model is destroyed. The engine itself outlives the
  models, which is safe: it holds no model by ownership, and QML wrappers clear
  themselves when their object is destroyed.
- Command-line handling is `parseLaunchArguments(QStringList)`, which returns
  the parsed arguments or an exit status plus the text for stdout or stderr
  (`--help` is 0, a usage error is 1, matching `process()`). `main()` prints and
  returns. It never calls `exit()` with live Qt state.
- The route-construction probe no longer deletes the root objects. It quits the
  loop and lets the real shutdown run, so the existing stderr warning gate and the
  new open/close loop cover the production teardown.

## Consequences

Tests: `qindaqt.settings-qml-teardown` (destruction order with the guard, and a
negative control without it that must show the destruction handler losing its
model), `qindaqt.settings-launch-arguments` (status and text as values, bounded
arguments), `qindaqt.settings-app-help-exits` (the real binary: exit status,
the right stream, nothing else on the other) and `qindaqt.settings-app-shutdown-loop`
(twenty offscreen open/close cycles across six pages with a poisoned heap and a
warning gate).

Limits of the evidence, stated plainly: the field aborts did not reproduce in
an isolated offscreen sandbox or against a private Weston, before or after the
change (the sandbox has no live services, and core dumps are disabled
session-wide, so the field stacks are unavailable). The loop is therefore a
regression guard and the ordering hazard is demonstrated by the unit-level
negative control, not by reproducing a field abort. The same `process()`
pattern exists in other tools (qindanote and qindafox-launcher, per the stability findings) and is
not changed here.

## Revisit when

A field abort is captured with a usable stack that points elsewhere.
