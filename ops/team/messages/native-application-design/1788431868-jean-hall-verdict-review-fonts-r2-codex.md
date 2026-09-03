# Jean Hall — independent repair-descendant recheck

- Persona: Jean Hall, independent test-fixture reviewer
- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Exact candidate SHA: `b7b52083ab4bb11d36284020d42a6a6e854db260`
- Candidate tree SHA: `ff493f6df69fdae4484a9b41b7150ef4411dbc9d`
- Parent SHA: `cf2c2a9e00513b994c077cd451b774d1cbad0a34`
- Product ancestor: `bf1c83a15206c190d0c56950c261c1ada0196281`
- Base SHA: `ce9228d9694622d503d92a38d01986f8f124f188`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/controls-visual-fonts-codex-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-fonts-codex`

## Findings ledger

### P0

None.

### P1

None. Prior P1-1 is closed.

### P2

None. Prior P2-1 and P2-2 are closed.

### P3

None.

## Repair recheck

### Prior P1-1: high-contrast host-Noto bypass — closed

The repair rewrites every `data/themes/*.json` `fontFamily` and
`monoFontFamily` into build-root pinned copies before `publishTheme()` loads a
theme (`tests/controls/control_test_support.cpp:163-239`), while leaving the
product catalog unchanged. `tests/controls/tst_controls_font_pinning.cpp:167-209`
is a registered assertion that every pinned copy names `QindaQt Sans` /
`QindaQt Sans Mono` and otherwise equals its source object. The registered
canary at `tests/controls/CMakeLists.txt:105-129` applies the checked-in
host-Noto-hidden fontconfig to the exact formerly bypassing row.

Before rebuilding, I ran the stale ancestor-configured visual executable under
that candidate canary environment:

```sh
FONTCONFIG_FILE=$WORKTREE/tests/controls/fontconfig/no-noto/fonts.conf \
XDG_CACHE_HOME=$ROOT/fontconfig-no-noto-cache \
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
QT_SCALE_FACTOR=1.0 QT_SCALE_FACTOR_ROUNDING_POLICY=PassThrough \
QINDAQT_CONTROLS_TEST_SCALE=1.0 \
cmake -DQINDAQT_VISUAL_TEST=$ROOT/debug/tests/controls/qindaqt_controls_visual_tests \
  -DQINDAQT_SCALE=100 -DQINDAQT_ROW=qinda-high-contrast-compact \
  -P tests/controls/run_controls_visual_row.cmake
```

Observed on the `bf1c83a` binary: exit 1; 2 QtTest functions passed and 1
failed at `tests/controls/tst_controls_visual.cpp:227`, with the prior exact
`baseline drift: 145270 pixels, max channel delta 255`. This proves the new
registered environment is mutation-sensitive to the rejected implementation.

After the descendant rebuild:

```sh
ctest --test-dir $ROOT/debug \
  -R '^qindaqt\.controls-visual-no-noto-100-qinda-high-contrast-compact$' \
  --output-on-failure --no-tests=error
```

Observed: exit 0, 1/1 passed.

I also reran the prior verdict's exact scratch-fontconfig controls:

```sh
XDG_CACHE_HOME=$ROOT/fontconfig-no-noto-cache \
FONTCONFIG_FILE=$ROOT/fontconfig-no-noto/fonts.conf fc-cache -f
XDG_CACHE_HOME=$ROOT/fontconfig-no-noto-cache \
FONTCONFIG_FILE=$ROOT/fontconfig-no-noto/fonts.conf \
fc-list | rg '/usr/share/fonts/noto/'
XDG_CACHE_HOME=$ROOT/fontconfig-no-noto-cache \
FONTCONFIG_FILE=$ROOT/fontconfig-no-noto/fonts.conf fc-match -v 'Noto Sans'
FONTCONFIG_FILE=$ROOT/fontconfig-no-noto/fonts.conf \
XDG_CACHE_HOME=$ROOT/fontconfig-no-noto-cache \
ctest --test-dir $ROOT/debug \
  -R '^qindaqt\.controls-visual-100-qinda-light-compact$' \
  --output-on-failure --no-tests=error
FONTCONFIG_FILE=$ROOT/fontconfig-no-noto/fonts.conf \
XDG_CACHE_HOME=$ROOT/fontconfig-no-noto-cache \
ctest --test-dir $ROOT/debug \
  -R '^qindaqt\.controls-visual-100-qinda-high-contrast-compact$' \
  --output-on-failure --no-tests=error
```

Observed: `fc-cache` exit 0; the `fc-list | rg` negative query exited 1 with no
Noto path; `fc-match` resolved `Noto Sans` to
`/usr/share/fonts/liberation-fonts/LiberationSans-Regular.ttf`; both descendant
visual rows exited 0 with 1/1 passed. With the candidate's absolute checked-in
config, exact-family `fc-list` queries for `Noto Sans` and `Noto Sans Mono`
also returned no fonts, while `fc-match 'Noto Sans'` returned Liberation Sans
and `fc-match 'DejaVu Sans'` retained DejaVu Sans.

The requested non-proof control also remains stable:

```sh
FONTCONFIG_FILE=/dev/null ctest --test-dir $ROOT/debug \
  -R '^qindaqt\.controls-visual-100-qinda-high-contrast-compact$' \
  --output-on-failure --no-tests=error
```

Observed: exit 0, 1/1 passed. Consistent with the corrected documentation,
fontconfig's fallback behavior means this is only a compatibility check; the
checked-in no-Noto canary is the host-Noto exclusion proof.

### Prior P2-1: invalid sfnt checksums — closed

The registered `qindaqt.controls-font-pinning` row now validates every table
directory checksum and the whole-font `head.checkSumAdjustment` invariant for
all four files (`tests/controls/tst_controls_font_pinning.cpp:211-265`). It
passes in Debug and Release as part of the 4/4 font selectors below.

I reran the prior independent validator against the descendant files and
against copies extracted from `bf1c83a`:

```sh
python3 $ROOT/font-checksums.py
git archive bf1c83a tests/controls/fonts | tar -x -C $ROOT/r2-ancestor-fonts
python3 $ROOT/font-checksums.py \
  $ROOT/r2-ancestor-fonts/tests/controls/fonts/NotoSans-Regular.ttf \
  $ROOT/r2-ancestor-fonts/tests/controls/fonts/NotoSans-SemiBold.ttf \
  $ROOT/r2-ancestor-fonts/tests/controls/fonts/NotoSans-Bold.ttf \
  $ROOT/r2-ancestor-fonts/tests/controls/fonts/NotoSansMono-Regular.ttf
```

Observed: all four descendant files had `table_mismatches=[]` and whole-font
sum `0xb1b0afba`. All four ancestor files reproduced the old `name`-table
mismatch and non-magic whole-font sums (`0xb35f2301`, `0xaf50ec1f`,
`0xb2f3a4a0`, and `0xb47dd107`). Thus the registered checksum property would
fail on `bf1c83a`.

I also copied the four upstream host fonts into `$ROOT/r2-renewal`, ran the
candidate `tests/controls/fonts/rename_family_names.py` on those scratch files,
and reran the independent validator. The tool exited 0; all four outputs had no
table mismatch and the magic whole-font sum. `fc-scan` reported `QindaQt Sans`
Regular/SemiBold/Bold and `QindaQt Sans Mono` Regular.

### Prior P2-2: focused build could not run installed consumer — closed

`tests/controls/run_installed_controls_consumer.cmake:21-43` now requires and
uses `--component ControlsQmlModules`; the component declared at
`tests/controls/CMakeLists.txt:224-286` stages only Controls and Tokens. The
complete selector passed after the prescribed focused target build in both
configurations. In Release, neither
`src/profiles/libqindaqt_profiles.a` nor
`src/shell_layout/libqindaqt_shell_layout.a` exists, yet
`qindaqt.controls-installed-import` passed. The resulting staged tree contains
only the two QML modules and the test's copied consumer, so the old unscoped
whole-tree install dependency is gone.

## Configure, build, and test evidence

The exact Debug and Release configurations were rerun:

```sh
cmake -S . -B $ROOT/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
  -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON \
  -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF \
  -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
cmake -S . -B $ROOT/release -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
  -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON \
  -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF \
  -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Both exited 0. CMake emitted the pre-existing mixed-prefix runtime-path
warnings from the pinned dependency cache; configuration and generation
completed.

Focused build command, run once per configuration:

```sh
cmake --build $ROOT/{debug|release} --parallel 3 --target \
  qindaqt_controls_visual_tests qindaqt_controls_behavior_tests \
  qindaqt_controls_font_pinning_tests \
  qindaqt_controls_font_fixture_negative_tests \
  qindaqt_controls_memory_probe qindaqt_controls_bare_memory_probe \
  qindaqt_controls_qml qindaqt_tokens_qmlplugin \
  qindaqt_controls_qmlplugin
```

Both exited 0 with 26 incremental Ninja steps completed and no compiler
failure. `ctest -N -R '^qindaqt\.controls-'` reported exactly 34 tests.

Focused font selectors:

```sh
ctest --test-dir $ROOT/debug -R '^qindaqt\.controls-font-' \
  --output-on-failure --no-tests=error
ctest --test-dir $ROOT/release -R '^qindaqt\.controls-font-' \
  --output-on-failure --no-tests=error
```

Both exited 0, 4/4 passed.

Complete required stability matrix:

```sh
ctest --test-dir $ROOT/debug -R '^qindaqt\.controls-' \
  --output-on-failure --no-tests=error
ctest --test-dir $ROOT/debug -R '^qindaqt\.controls-' \
  --output-on-failure --no-tests=error
ctest --test-dir $ROOT/release -R '^qindaqt\.controls-' \
  --output-on-failure --no-tests=error
```

Observed, consecutively: Debug run 1 exit 0, 34/34; Debug run 2 exit 0,
34/34; Release exit 0, 34/34. Each run included 26/26 visual rows, including
the canary.

Manual scratch-copy missing-file reproduction:

```sh
mkdir -p $ROOT/r2-negative/fonts
cp -a tests/controls/fonts/NotoSans-{SemiBold,Bold}.ttf \
  tests/controls/fonts/NotoSansMono-Regular.ttf $ROOT/r2-negative/fonts/
ulimit -c 0
QT_QPA_PLATFORM=offscreen \
  $ROOT/debug/tests/controls/qindaqt_controls_font_fixture_negative_tests \
  missing $ROOT/r2-negative/fonts
```

Observed: expected exit 134 with
`byte-pinned visual font fixture is missing: .../NotoSans-Regular.ttf`.
The registered `qindaqt.controls-font-fixture-missing` row passed in both
profiles.

## Static, scope, and tree evidence

```sh
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir $ROOT/site
./tools/check-source-shape
git diff --check
git diff --check bf1c83a..b7b5208
git diff --check ce9228d..b7b5208 -- tests/controls docs
git diff --name-only bf1c83a..b7b5208 -- src
```

All commands exited 0. `validate-docs` validated 118 Markdown documents and
navigation; strict MkDocs built successfully; source shape checked 1,816 files
with zero skips and only the two pre-existing 500/539-line decomposition
warnings. The `src/` diff was empty. The repair changes no JSON, so the
changed-JSON `python3 -m json.tool` gate was not applicable. The repair diff
does not change any baseline PNG; the whole-candidate PNGs remain the set
reviewed for `bf1c83a`.

The supplied repair-lane `last-message.md` did not exist. I read the repair
brief and the newest timestamped handoff instead:
`1788424756-katherine-booth-handoff.md`.

No nested compositor/session row, host D-Bus service, hardware, uinput, or
network command was run.

Final identity and cleanliness checks again resolved HEAD to
`b7b52083ab4bb11d36284020d42a6a6e854db260`; `git status --porcelain` was
empty.

## Verdict

The repaired descendant closes the previous host-font bypass, invalid-font,
and focused-install defects with registered, mutation-sensitive evidence. It
satisfies the bounded Controls visual-gate review contract.

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/0
