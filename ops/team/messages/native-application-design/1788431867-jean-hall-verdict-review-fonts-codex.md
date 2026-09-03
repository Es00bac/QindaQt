# Jean Hall — independent test-fixture review

- Persona: Jean Hall, independent test-fixture reviewer
- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Exact candidate SHA: `bf1c83a15206c190d0c56950c261c1ada0196281`
- Candidate tree SHA: `572e5be107963f2a16a9322e86ce7c9835d9b6f4`
- Parent SHA: `f5b182ca3738f8a5b4de5fdf318b93b7a5a0e545`
- Base SHA: `ce9228d9694622d503d92a38d01986f8f124f188`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/controls-visual-fonts-codex-review`
- Assigned build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-fonts-codex`

## Findings ledger

### P0

None.

### P1

#### P1-1: The five high-contrast visual rows still render the host `Noto Sans`, so the claimed byte pin is incomplete

`data/themes/qinda-high-contrast.json:6-7` names `Noto Sans` and `Noto Sans Mono` directly. The fixture registers the vendored files as `QindaQt Sans` / `QindaQt Sans Mono`, but `tests/controls/control_test_support.cpp:164-166` substitutes only `Inter` and `JetBrains Mono`. Consequently, the four Inter-based themes use the registered family while Qinda High Contrast continues resolving the host Noto family. The new proof is blind to this because `tests/controls/tst_controls_font_pinning.cpp:72-90` requests and checks only `Inter` Regular.

Reproduction:

1. I created the scratch fontconfig file `/home/cabewse/work_SPaC3/builds/qindaqt/review-fonts-codex/fontconfig-no-noto/fonts.conf`, which includes DejaVu, Liberation, Hack, Corefonts, URW, and Noto Emoji but omits `/usr/share/fonts/noto`, and retained `/etc/fonts/conf.d` rendering rules.
2. I verified the exclusion:

```sh
FONTCONFIG_FILE=/home/cabewse/work_SPaC3/builds/qindaqt/review-fonts-codex/fontconfig-no-noto/fonts.conf fc-cache -f
FONTCONFIG_FILE=/home/cabewse/work_SPaC3/builds/qindaqt/review-fonts-codex/fontconfig-no-noto/fonts.conf fc-list | rg '/usr/share/fonts/noto/'
FONTCONFIG_FILE=/home/cabewse/work_SPaC3/builds/qindaqt/review-fonts-codex/fontconfig-no-noto/fonts.conf fc-match -v 'Noto Sans' | sed -n '1,30p'
```

Results: `fc-cache` exited 0; `fc-list` produced no `/usr/share/fonts/noto/` match; `fc-match` resolved the request to `/usr/share/fonts/liberation-fonts/LiberationSans-Regular.ttf`.

3. The control row proves the custom configuration itself preserves the intended pinned path:

```sh
FONTCONFIG_FILE=/home/cabewse/work_SPaC3/builds/qindaqt/review-fonts-codex/fontconfig-no-noto/fonts.conf \
  ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-fonts-codex/debug \
  -R '^qindaqt\.controls-visual-100-qinda-light-compact$' --output-on-failure --no-tests=error
```

Observed: exit 0, 1/1 passed.

4. The high-contrast row under the same no-Noto configuration:

```sh
FONTCONFIG_FILE=/home/cabewse/work_SPaC3/builds/qindaqt/review-fonts-codex/fontconfig-no-noto/fonts.conf \
  ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-fonts-codex/debug \
  -R '^qindaqt\.controls-visual-100-qinda-high-contrast-compact$' --output-on-failure --no-tests=error
```

Observed: exit 8, 0/1 passed. `tst_controls_visual.cpp:227` reported `baseline drift: 145270 pixels, max channel delta 255`.

Expected: 1/1 passed, just like the light control, because the lane contract says every visual row renders the registered repository-owned bytes and specifically requires the host-Noto-hidden run to pass.

The `/dev/null` command is not a contrary control: it passes only because fontconfig falls back to the standard host configuration, as the candidate documentation itself says. I ran:

```sh
FONTCONFIG_FILE=/dev/null ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-fonts-codex/debug \
  -R '^qindaqt\.controls-visual-100-qinda-high-contrast-compact$' --output-on-failure --no-tests=error
```

Observed: exit 0, 1/1 passed.

This also makes the statements at `docs/wiki/adr/0021-isolate-controls-visual-rows.md:73-85`, `docs/wiki/development/testing-harness.md:300-314`, and `docs/wiki/shell/controls.md:117-130` stronger than the implementation: a host Noto change can still alter all five high-contrast baselines.

### P2

#### P2-1: The checked-in renamed fonts and renewal tool leave invalid sfnt checksums

`tests/controls/fonts/rename_family_names.py:113-134` rebuilds the font container and changes the `name` table, its offset, and its length, but never recalculates the `name` table directory checksum or the `head.checkSumAdjustment` value. All four committed fixtures therefore fail the sfnt checksum invariants even though the current Qt/FreeType build permissively loads them.

Reproduction:

```sh
python3 /home/cabewse/work_SPaC3/builds/qindaqt/review-fonts-codex/font-checksums.py
```

Observed:

```text
NotoSans-Bold.ttf table_mismatches= [('name', '0xa5cac628', '0xa724c918')] whole_font= 0xb2f3a4a0 expected= 0xb1b0afba
NotoSans-Regular.ttf table_mismatches= [('name', '0xa9d6c8ce', '0xab9bcb53')] whole_font= 0xb35f2301 expected= 0xb1b0afba
NotoSans-SemiBold.ttf table_mismatches= [('name', '0xb8d9d7c6', '0xb690d6bb')] whole_font= 0xaf50ec1f expected= 0xb1b0afba
NotoSansMono-Regular.ttf table_mismatches= [('name', '0x892eadda', '0x8c13af09')] whole_font= 0xb47dd107 expected= 0xb1b0afba
```

Negative control using the four host originals:

```sh
python3 /home/cabewse/work_SPaC3/builds/qindaqt/review-fonts-codex/font-checksums.py \
  /usr/share/fonts/noto/NotoSans-Regular.ttf \
  /usr/share/fonts/noto/NotoSans-SemiBold.ttf \
  /usr/share/fonts/noto/NotoSans-Bold.ttf \
  /usr/share/fonts/noto/NotoSansMono-Regular.ttf
```

Observed: every original had `table_mismatches=[]` and whole-font checksum `0xb1b0afba`.

Expected: the renewal utility must update each changed table checksum and then set `head.checkSumAdjustment` so the rebuilt font remains a valid sfnt artifact. The bounded workaround is to renew the files with a standards-aware font writer or add these two checksum updates and a validator.

#### P2-2: The advertised 33-test Controls selector is not runnable after the prescribed focused target build

The review recipe requires building only the candidate's Controls targets before running `^qindaqt\.controls-`. However, `tests/controls/run_installed_controls_consumer.cmake:23-35` runs an unscoped whole-tree `cmake --install`, while `tests/controls/CMakeLists.txt:175-190` gives the test no build closure for unrelated install artifacts. In the fresh assigned build roots, the exact focused build succeeded, but the full selector failed in both configurations because unrelated libraries had not been built.

Reproduction after the successful focused builds listed below:

```sh
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-fonts-codex/debug \
  -R '^qindaqt\.controls-' --output-on-failure --no-tests=error
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-fonts-codex/release \
  -R '^qindaqt\.controls-' --output-on-failure --no-tests=error
```

Observed: both commands exited 8 with 32/33 passed. `qindaqt.controls-installed-import` failed because `src/profiles/libqindaqt_profiles.a` did not exist. After explicitly building `qindaqt_profiles` in Debug, that row still failed at the next unrelated artifact, `src/shell_layout/libqindaqt_shell_layout.a`.

Expected: 33/33 after the lane-prescribed focused target build. The handoff's 33/33 evidence came from a previously populated build tree and does not reproduce from the clean review build root. A bounded fix is to install a Controls-specific CMake component or provide a preparation target with the exact install closure.

### P3

None.

## Review-question evidence

### 1. Byte pinning

- `tests/controls/control_test_support.cpp:133-166` lists four fixtures, calls `QFontDatabase::addApplicationFont`, rejects registration failure, requires exactly the expected registered family, and substitutes `Inter` / `JetBrains Mono` to those families.
- The existing font rows all passed in both full-selector attempts: pinning plus missing, corrupt, and wrong-family controls were 4/4.
- A manual scratch-copy removal provided an independent negative control:

```sh
mkdir -p /home/cabewse/work_SPaC3/builds/qindaqt/review-fonts-codex/negative-manual/fonts
cp -a tests/controls/fonts/NotoSans-Regular.ttf tests/controls/fonts/NotoSans-SemiBold.ttf \
  tests/controls/fonts/NotoSans-Bold.ttf tests/controls/fonts/NotoSansMono-Regular.ttf \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-fonts-codex/negative-manual/fonts/
rm /home/cabewse/work_SPaC3/builds/qindaqt/review-fonts-codex/negative-manual/fonts/NotoSans-Regular.ttf
ulimit -c 0
QT_QPA_PLATFORM=offscreen \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-fonts-codex/debug/tests/controls/qindaqt_controls_font_fixture_negative_tests \
  missing /home/cabewse/work_SPaC3/builds/qindaqt/review-fonts-codex/negative-manual/fonts
```

Observed: exit 134 and the fatal diagnostic named the missing scratch path.
- `tests/controls/fonts/LICENSE-OFL.txt` contains the SIL OFL 1.1 text. `fc-scan` reports the expected repository-owned family/style names for all four files.
- The proportional set is Regular, SemiBold, and Bold; source inspection finds `Font.Normal`, `Font.DemiBold`, and `font.bold: true`, with no italic request. Mono Regular covers the schema's monospace substitution. No extra weights or styles are vendored.
- Overall answer: the Inter/JetBrains path is genuinely registered and fail-closed, but P1-1 proves the complete five-theme gate is not host independent.

### 2. Baseline honesty

I extracted every old image using:

```sh
git archive ce9228d tests/controls/baselines | \
  tar -x -C /home/cabewse/work_SPaC3/builds/qindaqt/review-fonts-codex/baseline-review/old
python3 /home/cabewse/work_SPaC3/builds/qindaqt/review-fonts-codex/baseline-review/review.py
```

The script uses the candidate test's own comparison definition: a pixel is changed when its maximum RGBA channel distance is greater than 2. All 25 old/new dimensions are identical. The measured old-to-new drift is:

| Baseline | Changed pixels | Percent | Max delta |
| --- | ---: | ---: | ---: |
| 100/dark compact | 5,864 | 1.6621% | 242 |
| 100/dark ordinary | 5,864 | 0.9696% | 242 |
| 100/dark large | 5,864 | 0.6464% | 242 |
| 100/dusk compact | 5,906 | 1.6740% | 242 |
| 100/dusk ordinary | 5,906 | 0.9765% | 242 |
| 100/dusk large | 5,906 | 0.6510% | 242 |
| 100/high-contrast compact | 6,016 | 1.7052% | 255 |
| 100/high-contrast ordinary | 6,016 | 0.9947% | 255 |
| 100/high-contrast large | 6,016 | 0.6631% | 255 |
| 100/light compact | 5,902 | 1.6729% | 255 |
| 100/light ordinary | 5,902 | 0.9759% | 255 |
| 100/light large | 5,902 | 0.6506% | 255 |
| 100/macos compact | 5,912 | 1.6757% | 255 |
| 100/macos ordinary | 5,912 | 0.9775% | 255 |
| 100/macos large | 5,912 | 0.6517% | 255 |
| 125/dark ordinary | 9,285 | 0.9825% | 242 |
| 125/dusk ordinary | 9,308 | 0.9850% | 242 |
| 125/high-contrast ordinary | 9,504 | 1.0057% | 255 |
| 125/light ordinary | 9,332 | 0.9875% | 255 |
| 125/macos ordinary | 9,350 | 0.9894% | 255 |
| 150/dark ordinary | 12,410 | 0.9120% | 242 |
| 150/dusk ordinary | 12,445 | 0.9145% | 242 |
| 150/high-contrast ordinary | 12,648 | 0.9295% | 255 |
| 150/light ordinary | 12,477 | 0.9169% | 255 |
| 150/macos ordinary | 12,502 | 0.9187% | 255 |

I inspected old/new/diff composites for light, dark, and high-contrast at all three scales:

- `/home/cabewse/work_SPaC3/builds/qindaqt/review-fonts-codex/baseline-review/inspection-100.png`
- `/home/cabewse/work_SPaC3/builds/qindaqt/review-fonts-codex/baseline-review/inspection-125.png`
- `/home/cabewse/work_SPaC3/builds/qindaqt/review-fonts-codex/baseline-review/inspection-150.png`

Every diff is confined to glyph shapes at unchanged text positions. Card/control geometry, colors, line breaks, and overall pixel dimensions are unchanged. No baseline-honesty finding.

### 3. Determinism

Normal-environment visual matrix:

```sh
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-fonts-codex/debug \
  -R '^qindaqt\.controls-visual-' --output-on-failure --no-tests=error
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-fonts-codex/debug \
  -R '^qindaqt\.controls-visual-' --output-on-failure --no-tests=error
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-fonts-codex/release \
  -R '^qindaqt\.controls-visual-' --output-on-failure --no-tests=error
```

Results: Debug run 1 exit 0, 25/25; immediately consecutive Debug run 2 exit 0, 25/25; Release exit 0, 25/25. The manual removed-font negative exited 134 as described above. P1-1 is the determinism failure under the required host-Noto-hidden control.

### 4. Scope

- Literal `git diff ce9228d..bf1c83a -- src` lists unrelated Bluetooth/global-menu files carried by `f5b182c`, exactly as the review brief warned. `git diff ce9228d..74b1246 -- src` and `git diff f5b182c..bf1c83a -- src` are both empty, so neither Controls product commit changes `src/`; the manager's stated plan to drop the synced parent content is required.
- `./tools/check-source-shape` exits 0 with 1,816 source files checked and no allowlisted skips; it prints two pre-existing decomposition warnings.
- ADR-0021 is preserved and amended: the base text remains, the status gains `amended 2026-09-02`, and a new dated section is appended.
- The candidate-owned diff contains no JSON, so there was no changed JSON file on which to run `python3 -m json.tool`.

## Configure, build, and static-gate results

Both exact configurations exited 0:

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-fonts-codex/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-fonts-codex/release -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Both focused builds exited 0 (118 Ninja steps apiece):

```sh
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-fonts-codex/debug --parallel 3 \
  --target qindaqt_controls_visual_tests qindaqt_controls_behavior_tests \
  qindaqt_controls_font_pinning_tests qindaqt_controls_font_fixture_negative_tests \
  qindaqt_controls_memory_probe qindaqt_controls_bare_memory_probe qindaqt_controls_qml \
  qindaqt_tokens_qmlplugin qindaqt_controls_qmlplugin
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-fonts-codex/release --parallel 3 \
  --target qindaqt_controls_visual_tests qindaqt_controls_behavior_tests \
  qindaqt_controls_font_pinning_tests qindaqt_controls_font_fixture_negative_tests \
  qindaqt_controls_memory_probe qindaqt_controls_bare_memory_probe qindaqt_controls_qml \
  qindaqt_tokens_qmlplugin qindaqt_controls_qmlplugin
```

Static gates:

```sh
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-fonts-codex/site
./tools/check-source-shape
git diff --check
```

Results: all exit 0. `validate-docs` validated 118 Markdown documents and navigation; strict MkDocs built successfully; source-shape checked 1,816 files; `git diff --check` was clean. A scoped `git diff --check ce9228d..bf1c83a -- tests/controls ...` also exited 0.

No nested compositor, host D-Bus service, hardware, uinput, or network row was run.

## Verdict

The candidate must not integrate as the byte-pinned host-independent Controls visual gate. Four theme families are pinned, but Qinda High Contrast bypasses the registered family, and the exact host-Noto-hidden reproduction fails. The malformed sfnt checksums and non-self-contained focused test build are additional repair items.

VERDICT REJECT P0/P1/P2/P3=0/1/2/0
