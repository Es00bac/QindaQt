# Exact-candidate review — Shell iconography I1

- **Reviewer:** Carolyn Bertozzi, independent shell reviewer (slug `carolyn-bertozzi`)
- **Provider/model:** Z.AI GLM, `zai-coding-plan/glm-5.3`
- **Candidate SHA:** `54cda1fbea6b4b043eaacef04a1f9d9ed7fed4c8` (`worker/shell-icons`, implementer Gerty Cori / Moonshot Kimi)
- **Candidate tree SHA:** `8b31c20851a8a3af18861a8683bb77c5701a8001`
- **Parent SHA:** `37f8523ed105e66d9784f8cca767d060eed3f1da`
- **Base SHA:** `37f8523ed105e66d9784f8cca767d060eed3f1da` (base-to-candidate diff reviewed in full: 27 files, +2909/-0, purely additive)
- **Review worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/shell-icons-glm-review` (detached HEAD at the candidate; `git status --porcelain` empty before and after review; no product file edited, committed, or staged)
- **Build root:** `/home/cabewse/work_SPaC3/builds/qindaqt/review-icons-glm` (system-KWin initial cache)
- **Date:** 2026-09-04

## Findings ledger

### P0

None.

### P1

**P1-1 Unbounded byte retention of hostile provider ids in the image cache.**

- **Where:** `src/shell/icons/src/icon_image_provider.cpp:71` (cache lookup keyed on the raw `id` including the query string), `:96-100` (evict/insert with the raw `id`), and `:131-139` (`parseRequest` bounds only the extracted `request.name` to the 128-byte grammar; the raw `id` used as cache key is never length-bounded).
- **Contract falsified:** `docs/wiki/shell/iconography.md`, Confinement contract, bounded-resources bullet: "… a 64-entry provider image LRU … Oversized or hostile inputs contribute nothing instead of growing shell memory." Echoed in `src/shell/icons/include/qindaqt/shell/icons/icon_image_provider.h:33-35` ("never a null image and never a warning" fail-closed doctrine; bounds listed in `icon_theme_limits.h`). The entry *count* is bounded at 64, but each entry's *key* is the caller-controlled URL id of arbitrary byte length, so hostile input grows shell memory without module-side bound.
- **Reachability:** any `Image`/`Icon` `source` built in the shell process from a long or corrupted name (the `Icon` element does not bound `name` either, `src/shell/icons/qml/Icon.qml:43-49`), including policy-confined applet QML if it may create image sources. Not remotely reachable, not host-affecting; resolution correctness, rendering, and canonical containment are unaffected — which is why this is a resource-contract violation rather than a confinement escape.
- **Reproduction** (scratch harness under the review build root, Debug and Release builds, both reproduced identically):

  ```
  cd /home/cabewse/work_SPaC3/builds/qindaqt/review-icons-glm/attack
  g++ -std=c++20 -fPIC attack_provider_mem.cpp -o attack_provider_mem \
      -I<worktree>/src/shell/icons/include -I<worktree>/src/shell/launcher/include \
      $(Qt6 includes) <buildroot>/debug/src/shell/icons/libqindaqt_shell_icons.a \
      <buildroot>/debug/src/shell/launcher/libqindaqt_shell_launcher.a \
      -lQt6Core -lQt6Gui -lQt6Qml -lQt6Quick -lQt6Svg
  env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS \
      QT_QPA_PLATFORM=offscreen ./attack_provider_mem ./fixtures
  ```

  Harness: constructs `IconImageProvider` over an empty fixture root, calls `requestImage` with 70 **distinct** hostile ids of 4 MiB chars each (`"000001-" + 'x'*4MiB + "?size=8"`), then drops every caller-side QString, calls `malloc_trim(0)` (to separate live retention from allocator caching), measures `/proc/self/status` VmRSS; then floods with 128 small distinct ids (evicting all big keys) and re-measures.

  Observed (Debug; Release identical):

  ```
  rss before=35688 kB
  rss after caller release + trim =561944 kB, delta vs before=526256 kB (513.9 MiB)
  rss after small-id eviction flood + trim =37308 kB (drop 512.3 MiB)
  RESULT hostile-id cache keys RETAINED (unbounded byte growth)
  ```

  Expected: retention independent of hostile id byte length (the documented bound). Observed: 513.9 MiB live after the caller released every buffer — ≈ exactly the 64-entry LRU holding the last 64 raw ids (64 × 8 MiB UTF-16) — and released only when evicted, proving the cache keys are the live retainer. A control run with 70 *identical* ids (cache hits after the first) retained only ~21 MiB, confirming the per-distinct-id mechanism.
- **Bounded fix direction (for the repair, not part of the verdict):** refuse to cache ids beyond a byte ceiling, or key the cache on the parsed `Request` (name/size/scale/symbolic/color) rather than the raw id.

### P2

None.

### P3

**P3-1 `icon_runtime.h` comment overclaims second-install behavior.**
`src/shell/icons/include/qindaqt/shell/icons/icon_runtime.h:17-19` says "Calling install() twice on one engine replaces the lookup locator … so install() refuses a second call". The implementation (`src/shell/icons/src/icon_runtime.cpp:19-21`) returns `false` before touching anything; nothing is replaced. The wiki page ("A second install on one engine is refused") matches the code; only the header comment is wrong.

**P3-2 The `Icon` element silently drops recolor for semi-transparent colors.**
`Icon.qml:48` appends `&color=` whenever `root.color.a > 0`; QML serializes alpha-carrying colors as `#aarrggbb` (probe: `Qt.rgba(1,0,0,0.5).toString()` → `#80ff0000`), which `parseColor` (`icon_image_provider.cpp:40-49`) refuses (size ≠ 7), so no recolor happens with no signal. The provider contract documents only `#rrggbb`, so the provider behaves as documented; the element's `color` property documentation does not mention the alpha drop.

**P3-3 "hicolor always last" does not hold when the 16-entry chain cap is full.**
`icon_theme_locator.cpp:147-151` appends `hicolor` only `if (chain.size() < kMaxThemeChainLength)`; with 16 distinct themes already flattened (e.g. ≥16 injected names — the constructor does not bound their count — or long inherits chains), hicolor is dropped rather than the cap exceeded. The fail-closed direction is safe (spec fallback lost, cap kept), but the wiki/ADR/locator-header phrasing "hicolor always last" and "never exceeds 16" cannot both hold at the boundary. Unreachable with the documented production chain (`breeze`).

**P3-4 SVG payload ceiling reuses the index constant undocumented.**
`icon_image_provider.cpp:199` bounds QSvgRenderer input with `kMaxThemeIndexBytes` (256 KiB), whose header (`icon_theme_limits.h:20-22`) and the wiki bounds list describe it as the "index.theme parsing ceiling"; neither documents an SVG byte ceiling. Harmless bound, misdocumented ownership.

## Review surface covered

1. **IconThemeLocator** — read in full against the XDG Icon Theme Specification subset claimed. Spec parsing (Directories, Size/Scale/Type/MinSize/MaxSize/Threshold, Inherits), closest-size rule with device-pixel distances and scale-aware exact pass, `-symbolic` preference through the whole chain, unthemed root fallback, canonical containment via `confinedFile` (both `../` declarations and symlink escapes refused, dangling links refused), bounded index parse (256 KiB read, 128-directory cap, 16-inherits cap), cycle guard and depth-8 cap, hicolor-last, deterministic root/declaration/extension ordering, per-thread instance rule. Hostile fixture roots built under the review build root and attacked with a 23-row harness (`attack_locator.cpp`): spec-realistic slashed subdirectories (`48x48/apps` — the breeze shape, not covered by the product fixtures), CRLF indexes, symlinked theme directories, dangling symlink icons, inverted MinSize/MaxSize, hostile `Inherits` parents (`../eviltheme`, spaced), oversized injected theme names, duplicate `hicolor` injection, dotted/leading-dot name grammar edges, scale-2-over-scale-1 distance matching — all 23 rows PASS in Debug and Release.
2. **DesktopEntryIconResolver** — app id → `Icon=` over injected roots only; hostile ids (`../`, absolute, embedded NUL, 300-char) refused; empty/spaced `Icon=` refused; first-root precedence, `/`→`-` id mapping, depth-4 and count/byte caps; reuses the launcher's public pure `DesktopEntryParser` (`QindaQt::ShellLauncher` is the pure Core-only target, verified in `src/shell/launcher/CMakeLists.txt:5-35`); `hidden` combined-flag semantics verified against the parser header.
3. **IconImageProvider / QML `Icon`** — device-size rendering (SVG at exact device size, raster dimension-checked before/after decode with 2,048-px ceiling), symbolic recolor preserves alpha and replaces RGB (pixel-asserted in the product suite and verified live), deterministic neutral placeholder (never null/empty, no warning under `QT_FATAL_WARNINGS=1` in the product offscreen rows), LRU refresh-on-hit, size/scale clamps, hostile URL ids. A pure-Qt probe (`attack_qml.cpp`) proved Qt 6.11.1 delivers the URL query string **verbatim** to `requestImage` (`id: [sym?size=32&symbolic=1&color=%23ff0000]`), closing the gap that the product QML row does not assert symbolic recolor end-to-end; combined with the live provider row this establishes the `Icon` element's recolor path works through the real engine. Tokens-only colors in `Icon.qml` (no hard-coded color); the C++ placeholder gray is the documented "neutral" mark. No `QIcon::fromTheme`, `QStandardPaths`, or home-path lookups anywhere in the module (grep-verified).
4. **Tests** — all four suites read line-by-line. Negative controls are real and would fail on a tree without the rule they prove (escaping `../`/symlink fixtures with files that exist outside; oversized index that would parse without the ceiling; hostile names that pass without the grammar; depth-19 chain that resolves without the cap; recolor pixel assertions that hold source pixels without recoloring). No vacuous or tautological-only rows found. One observation: the provider LRU bound is only proven behaviorally (results stay correct under churn), not as a memory bound — see P1-1 for why that gap mattered.
5. **Module boundaries / source shape** — `src/shell/icons` links exactly `QindaQt::ShellLauncher` + Qt Core/Gui/Qml/Quick/Svg, matching the new module-boundaries row; no D-Bus/KWin/LayerShell/status-notifier/shell-runtime linkage; no filesystem writes; production files ≤ 394 non-blank lines. Shared-registry edits (`src/CMakeLists.txt`, `tests/CMakeLists.txt`, `mkdocs.yml`, ADR index, module-boundaries table, testing-harness page) are purely additive single-purpose entries. Documentation (wiki page, ADR-0071, harness section, nav) was checked against the code; the four precision gaps are P3-1..P3-4.

## Commands and results (all actually run)

Environment for every test/attack run: host `DISPLAY`/`WAYLAND_DISPLAY`/`DBUS_SESSION_BUS_ADDRESS` unset; CTest rows additionally set `DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent` and offscreen rows `QT_QPA_PLATFORM=offscreen`, `QT_QUICK_BACKEND=software`, `QT_FATAL_WARNINGS=1` via CTest properties. No nested-compositor rows, no host buses, no hardware, no network; scratch fixtures and binaries only under the review build root.

| Command | Result |
| --- | --- |
| `git rev-parse HEAD` | `54cda1fbea6b4b043eaacef04a1f9d9ed7fed4c8` (matches candidate) |
| `git status --porcelain` (before and after review) | empty, exit 0 |
| `cmake -S . -B <ROOT>/debug -G Ninja -C …/qindaqt-system-kwin-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug …` (exact lane recipe) | exit 0 (12.6 s configure) |
| same with `-B <ROOT>/release -DCMAKE_BUILD_TYPE=Release` | exit 0 (12.5 s) |
| `cmake --build <ROOT>/debug --parallel 3 --target qindaqt_shell_icons qindaqt_shell_icons_locator_tests qindaqt_shell_icons_resolver_tests qindaqt_shell_icons_provider_tests qindaqt_shell_icons_qml_tests` | exit 0 (101/101 steps, strict warnings, zero warnings) |
| same for `<ROOT>/release` | exit 0 (101/101 steps, zero warnings) |
| `ctest --test-dir <ROOT>/debug -R '^qindaqt\.shell-icons-' --output-on-failure --no-tests=error` | **4/4 passed**, exit 0 (subtests: locator 29, resolver 10, provider 20, qml 3) |
| `ctest --test-dir <ROOT>/release -R '^qindaqt\.shell-icons-' …` | **4/4 passed**, exit 0 |
| `./tools/validate-docs` | "Validated 145 Markdown documents and mkdocs.yml navigation", exit 0 |
| `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir <ROOT>/site` | built in 2.26 s, exit 0 |
| `./tools/check-source-shape` | exit 0 |
| `git diff --check` | exit 0 |
| `python3 -m json.tool` on changed JSON | not applicable — the base-to-candidate diff changes no JSON files |
| `./attack_locator <fixtures>` (Debug- and Release-linked builds) | 23/23 PASS, exit 0 (both) |
| `./attack_qml` (pure-Qt probe) | query string delivered verbatim to provider id; opaque color `#ff0000`, alpha color `#80ff0000`; exit 0 |
| `./attack_provider_mem <fixtures>` (Debug and Release) | 513.9 MiB retained → released only after eviction, exit 1 (the P1-1 reproduction, both profiles) |

## Verdict

The confinement core of this module is genuinely good: canonical containment held against every escape I could construct, the spec subset is implemented faithfully (including the breeze-shaped slashed directories the product fixtures miss), bounds fail closed, tests are live, and documentation tracks behavior closely. But the provider's LRU cache keys on the raw unbounded URL id, which directly falsifies the module's documented resource bound — 70 hostile `requestImage` calls retain ~514 MiB — and that is a contract violation with a clean reproduction, not a style nit. The implementer's own handoff states the rule this breaks ("oversized input contributes nothing instead of growing shell memory"); the code enforces it everywhere except the one string it caches whole.

VERDICT REJECT P0/P1/P2/P3=0/1/0/4
