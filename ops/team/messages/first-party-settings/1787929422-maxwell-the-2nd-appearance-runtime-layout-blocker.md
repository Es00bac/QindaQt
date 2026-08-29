# Appearance dependency breaks Settings runtime layout

- Timestamp: 2026-08-28T09:03:42-06:00
- From: Maxwell the 2nd
- State: material P1 exact-candidate finding; review continues
- Exact candidate: `9a495aad63034a5fa02613df7ab0d17b9d920385`

## Exact artifact evidence

The candidate's existing built
`build/dev/src/apps/settings_center/qindaqt-settings` has a direct ELF
`NEEDED` entry for `libqindaqt_tokens_qml.so`, introduced through the static
Appearance library's public `QindaQt::TokensQml` link
(`src/apps/settings/appearance/CMakeLists.txt:25-31`). Its generated install
script changes the executable runpath to only `$ORIGIN:$ORIGIN/../lib`, while
Tokens installs to `lib/qt6/qml/QindaQt/Tokens`
(`src/design_tokens/CMakeLists.txt:68-83`). A normal dynamic loader does not
recursively search that QML module directory, so the installed executable can
fail before `main()` — including the existing Notifications route.

The QML path has a second layout contradiction. `main.cpp:72-78` adds
`applicationDirPath()/../qml`; the observed build executable is under
`build/dev/src/apps/settings_center`, while generated QindaQt modules are under
`build/dev/qml`. Installed modules are under `lib/qt6/qml`, not `qml` beside
the prefix. Qt's scanner records `QindaQt.SettingsApp.Appearance` as a module
without a resolved build path. Staged or relocated installations therefore
cannot resolve the module even after the loader defect is repaired.

The exact candidate has neither a build-executable Appearance launch test nor
an installed/staged Settings runtime row. Repair needs explicit build/install
QML roots, a loader-safe runpath or non-direct module boundary, and a sanitized
installed runtime regression covering both routes. Product worktree remains
untouched.
