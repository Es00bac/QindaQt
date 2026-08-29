# Repaired candidate handoff: QST-1 semantic design tokens

- **Timestamp:** 2026-08-27T19:25:17Z
- **Worker:** Mara Voss — QindaQt Design Systems Engineer
- **Exact repaired commit for review:**
  `d891adeab694f0fea319cb728bb446bc74967ae9`
- **Direct parent / rejected candidate:**
  `73dd763e52c132cd5c7f629e697fb93a92392b3a`
- **Original exact base:** `dc29c88911f0ed6d381211027f16f46bbf92a07c`
- **Branch/worktree:** `worker/design-tokens-s1` at
  `/home/cabewse/work_SPaC3/container-wm-workers/design-tokens-s1`
- **Tree state:** clean after one new non-amended repair commit
- **Repair claim:**
  [`1787857485-mara-voss-design-tokens-repair-claim.md`](1787857485-mara-voss-design-tokens-repair-claim.md)
- **Checkpoint:**
  [`1787857863-mara-voss-design-tokens-repair-checkpoint.md`](1787857863-mara-voss-design-tokens-repair-checkpoint.md)
- **Cross-lane decision:**
  [`../desktop-experience-coordination/1787857864-mara-voss-qst1-transparency-decision.md`](../desktop-experience-coordination/1787857864-mara-voss-qst1-transparency-decision.md)

## Blocking findings repaired

### Total reduced-transparency projection

QST-1 now makes every loader-valid schema-v1 semantic color opaque before
deriving roles. It chooses a deterministic black/white desktop backdrop from
the authored canvas RGB luminance, then flattens canvas, surface, and raised
surface in order. Border, text, muted text, accent, and danger flatten over the
opaque surface; accent text flattens over the opaque accent. Disabled, subtle,
hover, pressed, focus, outline, status, and danger foreground roles then derive
from that opaque palette. Elevation blur/shadow remains disabled.

The policy is normative in the owning wiki and ADR consequence. Theme schema
v1 remains unchanged and still accepts Qt colors with alpha. Settings, controls,
shell, and applications receive no fallback or dependency change.

A public `ThemeLoader::fromJson` fixture authors alpha in all nine required
colors, pins stable HexArgb values for the semantic stack, and asserts alpha
255 for all 22 published QST color roles. A property pass repeats derivation
for every alpha value 0 through 255 and checks total deterministic output.

### Installed C++ contract gate

The checked-in consumer no longer tests an unexplained cardinality. It checks
the exact 15 top-level metadata/group keys, every direct group key set, all
status-pair/elevation-level key sets, QST revision 1, Qinda macOS identity and
mist/sage/text values, plus reduced-motion, reduced-transparency, type-scale,
radius, focus-fallback, and elevation behavior.

`qindaqt.design-tokens-installed-cpp-consumer` now performs a clean staged
install on every CTest run, configures a standalone CMake project against only
the installed headers/static libraries, builds the checked-in consumer, and
runs it against the installed Qinda macOS theme. This raises the focused and
broad registries to 5 and 88 respectively; broad green can no longer hide this
package failure.

## Changed paths in the repair commit

- Provider policy: `src/design_tokens/src/token_deriver.cpp`
- Focused/package evidence: `tests/design_tokens/CMakeLists.txt`,
  `tests/design_tokens/tst_derivation.cpp`,
  `tests/design_tokens/installed_cpp_consumer.cpp`,
  `tests/design_tokens/run_installed_cpp_consumer.cmake`, and
  `tests/design_tokens/installed_consumer/CMakeLists.txt`
- Normative/truth docs: `docs/wiki/architecture/design-tokens.md`,
  `docs/wiki/adr/0013-own-qst1-semantic-tokens.md`, theme-schema reference,
  testing harness, and implementation roadmap

No theme implementation/data, profile, Settings1, control, shell, application,
service, or platform path changed.

## Acceptance evidence

All commands below exited 0.

### Fresh Debug and Release

Both fresh no-KWin/no-shell configurations used strict warnings and completed
591 build steps:

```sh
cmake -S . -B build/design-tokens-repair-{debug,release} -G Ninja \
  -DCMAKE_BUILD_TYPE={Debug,Release} -DQINDAQT_BUILD_KWIN_PLUGIN=OFF \
  -DQINDAQT_BUILD_PRODUCTION_SHELL=OFF -DQINDAQT_BUILD_SHELL=OFF
cmake --build build/design-tokens-repair-{debug,release} --parallel 2
ctest --test-dir build/design-tokens-repair-{debug,release} \
  -R '^qindaqt[.]design-tokens-' --output-on-failure
ctest --test-dir build/design-tokens-repair-{debug,release} \
  --output-on-failure -j4
cmake --build build/design-tokens-repair-{debug,release} \
  --target all_qmllint --parallel 2
```

- Debug focused: **5/5 passed**; complete registry: **88/88 passed**.
- Release focused: **5/5 passed**; complete registry: **88/88 passed**.
- Both token QML lint targets passed (`Nothing to do` for the C++-only module).
- The focused derivation test includes exact built-in WCAG/Qinda macOS and new
  loader-valid translucent/property coverage; existing QML/thread/publication
  and performance contracts remain green.

### Fresh production-shell configuration

```sh
cmake -S . -B build/design-tokens-repair-production -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DQINDAQT_BUILD_KWIN_PLUGIN=OFF \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON
cmake --build build/design-tokens-repair-production --parallel 2
ctest --test-dir build/design-tokens-repair-production \
  -R '^qindaqt[.]design-tokens-' --output-on-failure
cmake --build build/design-tokens-repair-production \
  --target all_qmllint --parallel 2
```

- Fresh production build: **799 steps passed**.
- Production focused QST-1: **5/5 passed**.
- Production `all_qmllint`: exit 0. It repeats pre-existing shell-QML warnings
  outside this candidate; the token target itself passed.

### Installed QML and C++ consumers

```sh
cmake --install build/design-tokens-repair-release \
  --prefix build/design-tokens-repair-stage
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  /usr/lib/qt6/bin/qmltestrunner \
  -import "$PWD/build/design-tokens-repair-stage/lib/qt6/qml" \
  -input "$PWD/tests/design_tokens/qml/tst_installed_tokens.qml"
build/design-tokens-repair-release/tests/design_tokens/installed-cpp-stage/consumer-build/qindaqt_installed_design_token_consumer \
  build/design-tokens-repair-release/tests/design_tokens/installed-cpp-stage/share/qindaqt/themes/qinda-macos.json
```

- Standalone installed QML import/read-only boundary: **3/3 passed**.
- Standalone installed C++ exact-contract consumer: **exit 0**.
- The same C++ path also passed as a clean-stage CTest in Debug, Release, and
  production configurations.

### Performance, source, and documentation

Twenty iterations each, with 1,000 all-five-theme batches per iteration:

- Debug median: **28.5 ms / 1,000 = 0.0285 ms per batch**.
- Release median: **10.8 ms / 1,000 = 0.0108 ms per batch**.

These are recorded measurements below the documented 1 ms target, not a flaky
absolute CI assertion.

- `./tools/check-source-shape`: **726 files, zero violations**.
- `./tools/validate-docs`: **42 Markdown documents/navigation passed**; this is
  the repository link checker.
- `uvx --from mkdocs mkdocs build --strict`: passed.
- `git diff --cached --check`: passed before commit.
- Final `git status --porcelain=v1`: empty.

## Bounded caveats and non-claims

- The pre-existing repository still collects targets into `QindaQtTargets`
  without installing a project-wide `QindaQtConfig.cmake`/export file. This
  repair stays within its assigned boundary and proves direct installed public
  headers/static libraries and the installed QML module through standalone
  consumers.
- Production QML lint's unrelated existing shell warnings are unchanged.
- This remains a value/provider and software-renderer/package slice. It does
  not claim visual controls, Settings Center composition, a live accessibility
  bridge, desktop/compositor interaction, host input, physical display/GPU,
  memory, repaint, or complete application evidence.

## Requested next action

Iris Quill should recheck the exact repaired commit
`d891adeab694f0fea319cb728bb446bc74967ae9`, including both prior P1
reproductions, the clean staged CTest, normative flattening order, five-theme
WCAG/Qinda macOS preservation, and the full acceptance matrix above. Approval
must name this exact hash; the rejected parent remains unapproved.

