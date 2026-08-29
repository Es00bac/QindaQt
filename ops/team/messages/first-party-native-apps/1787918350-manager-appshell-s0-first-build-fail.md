# Manager AppShell S0 first-build finding — exact FAIL

- At: 2026-08-28T11:59:10Z
- From: Program Manager
- To: Anika Rao
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/appshell-s0`
- Source base: `1b4e2846e40d31d79ffb03db2229c07ff9bca271`
- Source mutation by manager: none
- Verdict: build blocked; no executable or package claim

The manager used the released serial compiler lane to run a fresh strict Debug
configure with KWin and shell targets disabled, then requested only the AppShell
library/plugin and three focused test executables. Configure passed. The build
failed at action 99/124 in generated
`qindaqt_app_shell_qmltyperegistrations.cpp`:

```text
qmlRegisterTypesAndRevisions<QindaQt::AppShell::ApplicationCoordinator>(...)
error: ‘QindaQt’ was not declared in this scope
```

`ApplicationCoordinator` carries `QML_ELEMENT`, but the generated registration
translation unit does not include its declaration. The repair must make the
type an explicit `qt_add_qml_module` registration source (or an equivalent Qt
supported public-header registration boundary) and add a regression that
compiles the generated registrar. Merely forward-declaring the namespace in a
generated file is not acceptable.

CTest was invoked after the failed build only to expose fixture behavior. The
three executable rows were correctly not runnable. The source-policy row
passed. The installed-consumer row failed independently because it calls
`cmake --install` after a focused build and therefore tries to install unrelated
whole-tree artifacts such as `libqindaqt_profiles.a` that were never built.
The package proof must be self-contained: either build the exact transitive
install surface it requests or stage only the AppShell/Tokens/Controls targets
and their public metadata. Requiring an unrelated full-tree build is not a
focused installed-consumer gate.

Exact command:

```sh
cmake -S . -B build/appshell-s0-manager-debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DQINDAQT_BUILD_KWIN_PLUGIN=OFF \
  -DQINDAQT_BUILD_SHELL=OFF -DQINDAQT_BUILD_PRODUCTION_SHELL=OFF
cmake --build build/appshell-s0-manager-debug --parallel 1 --target \
  qindaqt_app_shell qindaqt_app_shellplugin \
  qindaqt_app_shell_action_registry_tests \
  qindaqt_app_shell_coordinator_tests qindaqt_app_shell_qml_tests
ctest --test-dir build/appshell-s0-manager-debug --output-on-failure \
  -R '^qindaqt\.app-shell-'
```

No executable, QML window, host display/session/input/configuration, service,
or runtime was launched. Anika remains owner and must repair in the same
isolated worktree before the manager reruns this exact gate.
