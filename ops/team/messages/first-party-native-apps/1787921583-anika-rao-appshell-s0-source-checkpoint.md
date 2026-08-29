# Anika Rao AppShell S0 immutable source/static checkpoint

- Time: 2026-08-28T12:53:03Z
- Exact commit: `de52a04966763cc11f8a551c58bd76ca38694c5c`
- Tree: `c5a9e591314d4f3cd755a6595ca949f6ff0dc85c`
- Parent/base: `1b4e2846e40d31d79ffb03db2229c07ff9bca271`
- Branch/worktree: `worker/appshell-s0` at
  `/home/cabewse/work_SPaC3/container-wm-workers/appshell-s0`
- Exact sorted name-status manifest SHA-256:
  `f706c7d996504c333e480ddd0e940dc8d37dc56fa41ef5ee4cf45286bd4b55f7`
- Worktree: clean
- Verdict: source/static checkpoint only; not acceptance

## Exact 26-path manifest

```text
A	docs/wiki/adr/0027-extract-a-narrow-first-party-application-shell.md
M	docs/wiki/adr/index.md
A	docs/wiki/apps/application-shell.md
M	docs/wiki/architecture/module-boundaries.md
M	docs/wiki/index.md
M	mkdocs.yml
M	src/CMakeLists.txt
A	src/app_shell/CMakeLists.txt
A	src/app_shell/include/qindaqt/app_shell/action_registry.h
A	src/app_shell/include/qindaqt/app_shell/app_shell_types.h
A	src/app_shell/include/qindaqt/app_shell/application_coordinator.h
A	src/app_shell/qml/ApplicationShell.qml
A	src/app_shell/src/action_registry.cpp
A	src/app_shell/src/app_shell_types.cpp
A	src/app_shell/src/application_coordinator.cpp
M	tests/CMakeLists.txt
A	tests/app_shell/CMakeLists.txt
A	tests/app_shell/check_app_shell_source_policy.cmake
A	tests/app_shell/installed_consumer/CMakeLists.txt
A	tests/app_shell/installed_cpp_consumer.cpp
A	tests/app_shell/qml/AppShellTestScene.qml
A	tests/app_shell/qml/tst_installed_app_shell.qml
A	tests/app_shell/run_installed_app_shell_consumer.cmake
A	tests/app_shell/tst_action_registry.cpp
A	tests/app_shell/tst_application_coordinator.cpp
A	tests/app_shell/tst_application_shell.cpp
```

## Exact evidence

All of these exited 0 on the committed content immediately before commit:

- `git diff --cached --check`;
- AppShell source-policy CMake script;
- `qmlformat` parsing of all three AppShell-owned QML files;
- `tools/check-source-shape` — 998 source files checked;
- `tools/validate-docs` — 65 Markdown documents/navigation entries;
- `uvx --from mkdocs mkdocs build --strict` — completed in 0.49 seconds.

The earlier repair build established registrar compilation and 4/5 focused
rows, including the 3.79-second focused staged consumer, but that was before
the final accessibility source correction. Therefore it is context, not
executable evidence for this exact commit.

## Required next actions

1. Rowan Lee independently reviews this exact immutable commit at source level
   and returns concrete path/line findings, without accepting summaries.
2. Anika repairs any blocking finding as a non-amended descendant and Rowan
   rereviews that exact descendant.
3. After Rhea releases the serialized lane, Anika runs the manager's exact
   serial target build and all five `^qindaqt\.app-shell-` rows on the final
   exact candidate, reruns static gates, and hands off for independent
   executable review.

No milestone, acceptance, integrated product progress, UI session, service,
compositor, host input/configuration, or physical-device claim is made.
