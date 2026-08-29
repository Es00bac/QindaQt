# Candidate handoff: QST-1 semantic design tokens

- **Timestamp:** 2026-08-27T18:41:47Z
- **Worker:** Mara Voss — QindaQt Design Systems Engineer
- **Exact commit for review:**
  `73dd763e52c132cd5c7f629e697fb93a92392b3a`
- **Exact base:** `dc29c88911f0ed6d381211027f16f46bbf92a07c`
- **Branch/worktree:** `worker/design-tokens-s1` at
  `/home/cabewse/work_SPaC3/container-wm-workers/design-tokens-s1`
- **Tree state:** clean after the commit
- **Claim:**
  [`1787854244-mara-voss-design-tokens-claim.md`](1787854244-mara-voss-design-tokens-claim.md)
- **Owning design:**
  [`1787853515-juno-park-design-handoff.md`](1787853515-juno-park-design-handoff.md)
- **Schema-v1 boundary reply:**
  [`1787854245-mara-voss-qst1-theme-boundary-reply.md`](1787854245-mara-voss-qst1-theme-boundary-reply.md)

## User-visible outcome delivered

QST-1 is now a complete modular semantic-token boundary. A pure C++20 deriver
produces immutable, complete token generations from a public schema-v1
`ThemeSpec` and explicit normalized typography/accessibility inputs. The
separate `QindaQt.Tokens 1.0` GUI-thread singleton exposes read-only role maps
to QML and atomically publishes one aggregate change after a complete value
swap. Same-value publication is suppressed; null, invalid-theme, and
wrong-thread requests retain the last confirmed generation.

Theme schema v1 is unchanged. Theme/catalog selection, high-contrast palette
selection, Settings1 projection, persistence, shell/application code, services,
and Kirigami remain outside this module. ADR-0013 accepts QST-1 ownership and
permits any future Kirigami reuse only behind a QindaQt controls adapter.

## Changed paths

- New production module: `src/design_tokens/**`
- New focused/package tests: `tests/design_tokens/**`
- Additive registries: `src/CMakeLists.txt`, `tests/CMakeLists.txt`, `mkdocs.yml`
- New normative docs: `docs/wiki/architecture/design-tokens.md`,
  `docs/wiki/adr/0013-own-qst1-semantic-tokens.md`
- Additive truth updates: ADR index, wiki index, module boundaries, coding
  practices, implementation roadmap, testing harness, and theme-schema-v1
  reference

No `src/themes`, `tests/themes`, `data/themes`, profiles, Settings1, service,
shell, application, or platform-service path changed.

## Acceptance evidence

All commands exited 0 unless explicitly described as measurement output.

### Build and focused tests

```sh
cmake -S . -B build/design-tokens-debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DQINDAQT_BUILD_KWIN_PLUGIN=OFF \
  -DQINDAQT_BUILD_PRODUCTION_SHELL=OFF -DQINDAQT_BUILD_SHELL=OFF
cmake --build build/design-tokens-debug --parallel 4
ctest --test-dir build/design-tokens-debug \
  -R '^qindaqt\.design-tokens-' --output-on-failure

cmake -S . -B build/design-tokens-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DQINDAQT_BUILD_KWIN_PLUGIN=OFF \
  -DQINDAQT_BUILD_PRODUCTION_SHELL=OFF -DQINDAQT_BUILD_SHELL=OFF
cmake --build build/design-tokens-release --parallel 4
ctest --test-dir build/design-tokens-release \
  -R '^qindaqt\.design-tokens-' --output-on-failure
```

- Debug focused: **4/4 passed**.
- Release focused: **4/4 passed**.
- The selectors cover property/metric boundaries, total deterministic input
  normalization, reduced motion/transparency, text scale, high-contrast focus,
  exact documented WCAG pairs for all five built-ins, offscreen singleton
  consumption/publication, same-value suppression, wrong-thread rejection, and
  the record-only benchmark.
- Strict compiler warnings passed in both configurations.

### Broad regression suites

```sh
ctest --test-dir build/design-tokens-debug --output-on-failure -j4
ctest --test-dir build/design-tokens-release --output-on-failure -j4
```

- Debug: **87/87 passed**.
- Release: **87/87 passed**.
- These configurations intentionally disable the KWin plugin and shell. Their
  private D-Bus plus isolated Wayland/Weston tests ran; no user session or host
  input was touched.

### Non-flaky performance record

```sh
build/design-tokens-debug/tests/design_tokens/qindaqt_design_token_benchmark \
  -iterations 20 -median 5
build/design-tokens-release/tests/design_tokens/qindaqt_design_token_benchmark \
  -iterations 20 -median 5
```

Each timed iteration derives 1,000 complete five-theme batches. Final medians:

- Debug: **23.5 ms / 1,000 = 0.0235 ms per five-theme batch**.
- Release: **7.90 ms / 1,000 = 0.00790 ms per five-theme batch**.

The test records measurements and has no unstable absolute wall-clock CI
assertion. Both are below the documented 1 ms batch target on this host.

### Staged install and consumer proof

```sh
cmake --install build/design-tokens-release \
  --prefix "$PWD/build/design-tokens-stage-final"
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  /usr/lib/qt6/bin/qmltestrunner \
  -import "$PWD/build/design-tokens-stage-final/lib/qt6/qml" \
  -input "$PWD/tests/design_tokens/qml/tst_installed_tokens.qml"
c++ -std=c++20 -fPIC $(pkg-config --cflags Qt6Core Qt6Gui) \
  -I"$PWD/build/design-tokens-stage-final/include" \
  tests/design_tokens/installed_cpp_consumer.cpp \
  "$PWD/build/design-tokens-stage-final/lib/libqindaqt_design_tokens.a" \
  "$PWD/build/design-tokens-stage-final/lib/libqindaqt_themes.a" \
  $(pkg-config --libs Qt6Core Qt6Gui) \
  -o build/design-tokens-stage-final/installed_cpp_consumer
build/design-tokens-stage-final/installed_cpp_consumer \
  build/design-tokens-stage-final/share/qindaqt/themes/qinda-macos.json
```

- Installed QML import: **3/3 passed**.
- Installed public-header/static-library consumer: exit **0**.
- Staged payload includes four public headers, the pure library, backing QML
  library/plugin, `qmldir`, and generated `.qmltypes` under the Qt 6 QML path.
- Both targets participate in the repository's existing `QindaQtTargets`
  export set.

### Repository gates

- Debug and Release `all_qmllint`: passed. This C++-only QML module reports
  `Nothing to do`; the generated type data and installed runtime import are
  separately proven above.
- `./tools/check-source-shape`: **724 files checked, zero violations**; largest
  new production file is 240 physical lines, well below decomposition review.
- `./tools/validate-docs`: **42 Markdown documents/navigation passed**.
- `uvx --from mkdocs mkdocs build --strict`: passed.
- `git diff --cached --check`: passed after final staging.

## Bounded caveats and explicit non-claims

- The base repository collects targets into `QindaQtTargets` but does not yet
  install a project-wide `QindaQtConfig.cmake`/export file. This candidate does
  not change that shared package boundary outside its assigned paths; direct
  staged C++ and QML consumers are proven.
- ADR-0013 intentionally follows the in-flight Settings1 candidate's reserved
  ADR-0012. Integrate after/with that accepted boundary or preserve the number
  reservation during cherry-pick.
- S1 contains values and a QML adapter, not visual controls. Therefore no
  visual baseline is manufactured here; S2 controls owns five-theme visual and
  accessibility-tree baselines.
- No Settings Center, Settings1 subscription, shell integration, live desktop,
  compositor, host input, AT bridge, physical display, repaint, memory, or
  production-startup evidence is claimed.

## Requested next action

Assign a different worker to review the exact commit
`73dd763e52c132cd5c7f629e697fb93a92392b3a`. Review the QST formulas and WCAG
scope, public ownership/lifetime/thread/error/compatibility contract, QML
immutability/publication semantics, CMake install locations/export-set
participation, source decomposition, and documentation truth. If accepted, the
manager should integrate this commit, rerun affected gates on the integrated
tree, then open S2 `QindaQt.Controls` from the integrated exact base.

