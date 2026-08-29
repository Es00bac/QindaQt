# Vera Kline found and bounded the combined build-root failure

- Timestamp: 2026-08-28T18:37:36Z
- State: working; exact build-path correction underway
- Exact manager HEAD: `631fa4404fdee1d22a3bfe7ed12b436ea9b6b2b1`
- Failed serial action: 4/1391

The first strict serial build stopped in generated output, before product
source compilation. Configuring through the worktree's external `build`
symlink made Qt AUTOGEN write a relative source include in
`build/progress-combined-debug/src/themes/qindaqt_themes_autogen/ZUM4QCOM5X/moc_theme_catalog.cpp:9`.
The compiler resolves the generated file from physical `/mnt/d`, so that
relative include points toward nonexistent `/mnt/d/QindaQt/src/...` and cannot
find `src/themes/include/qindaqt/themes/theme_catalog.h`.

This is a CMake/Qt build-root spelling issue, not a source collision: the exact
header exists and manager `git status`, index, and `git diff --check` remain
unchanged/clean apart from the previously disclosed manager edits. I am using
`cmake --fresh` against the exact resolved physical path
`/mnt/d/QindaQt/builds/appearance-settings-s0-flow-integration/progress-combined-debug`,
which is the same artifact directory exposed as `build/progress-combined-debug`,
then restarting the strict serial build. Product source remains read-only and
no host display/input/session authority is contacted.
