# Handoff — Shell iconography I1 bounded repair of 54cda1fb (Gerty Cori)

- **Candidate SHA:** `288574a8fcc673859bbbf12e1f1c2b38c8c6ebdc`
- **Candidate tree SHA:** `d5d19409b1b227d6a1f6dd7535059e99a123afea`
- **Parent SHA:** `437d3b54` (prior handoff record on the rejected candidate)
- **Exact base SHA:** `37f8523ed105e66d9784f8cca767d060eed3f1da`
- **Branch:** `worker/shell-icons`
- **Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/shell-icons`
- **Date:** 2026-09-04
- **Addresses:** Carolyn Bertozzi's verdict REJECT 0/1/0/4 on `54cda1fb`
  (`/home/cabewse/work_SPaC3/builds/qindaqt/lanes/review-icons-glm/verdict.md`)

## What changed (vs `437d3b54`)

Sorted changed paths:

```
docs/wiki/adr/0071-shell-iconography-confined-xdg-icon-themes.md -> docs/wiki/adr/0072-shell-iconography-confined-xdg-icon-themes.md
docs/wiki/adr/index.md
docs/wiki/architecture/module-boundaries.md
docs/wiki/development/testing-harness.md
docs/wiki/shell/iconography.md
mkdocs.yml
src/shell/icons/include/qindaqt/shell/icons/icon_image_provider.h
src/shell/icons/include/qindaqt/shell/icons/icon_runtime.h
src/shell/icons/include/qindaqt/shell/icons/icon_theme_limits.h
src/shell/icons/include/qindaqt/shell/icons/icon_theme_locator.h
src/shell/icons/qml/Icon.qml
src/shell/icons/src/icon_image_provider.cpp
src/shell/icons/src/icon_theme_locator.cpp
tests/shell/icons/tst_shell_icons_locator.cpp
tests/shell/icons/tst_shell_icons_provider.cpp
```

**P1-1 repair (cause-level):** `requestImage` refuses any id over
`kMaxRequestIdUtf8Bytes` (1024, new documented constant) before parsing and
before any cache access. The LRU is now keyed on the parsed, bounded request
tuple (name, size, scale, color, symbolic) via `cacheKeyFor`; invalid
requests share one non-colliding sentinel key, and spellings parsing to the
same tuple share one entry. `Icon.qml` bounds `name` with the same grammar
(`effectiveName`), so over-long or out-of-grammar names never reach the URL
layer. Public API additions only: `cacheEntryCount()` / `cacheKeyBytes()`
observability seams for the bounded-resources rows; no existing signature
changed.

**P3-1:** `icon_runtime.h` second-install comment now matches the
refuse-and-change-nothing behavior.
**P3-2:** `Icon.color` documentation (QML + wiki) states the opaque-only
recolor rule: alpha-carrying colors are not recolor targets and the symbolic
icon keeps its own pixels.
**P3-3:** when the 16-entry chain cap is full, the deepest entry yields so
`hicolor` stays last; covered by `hicolorStaysLastWhenChainCapFull`.
**P3-4:** SVG payload ceiling is now the separately documented
`kMaxSvgSourceBytes` (same 256 KiB magnitude).

**ADR renumber:** 0071 → 0072 (main carries ADR-0071 for shell token
readiness): file renamed, index row, mkdocs nav, and every reference in
`iconography.md`, `testing-harness.md`, and `module-boundaries.md` updated.
ADR content otherwise unchanged.

## New rows (would fail on `54cda1fb`)

- `overlongIdRefusedBeforeCache`, `canonicalIdsShareOneCacheEntry`,
  `hostileIdFloodKeepsCacheKeysBounded` (entry count + retained key bytes),
  `hostileIdFloodKeepsRssBounded` (RSS delta, 64 MiB generous ceiling),
  `hicolorStaysLastWhenChainCapFull` (locator).
- Negative controls verified live against the unrepaired `54cda1fb` sources
  (scratch harnesses under my own build root `attack-old/`, old sources
  extracted via `git show`): the RSS-flood row logic retains
  **329,272 KiB** and exits 1; the hicolor-cap row loses the hicolor
  fallback and exits 1. Both pass on the repaired tree.

## Commands run for evidence (all actually run)

Environment for every test run: `env -u DISPLAY -u WAYLAND_DISPLAY -u
DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent`;
GUI rows additionally `QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software
QT_FATAL_WARNINGS=1` (per CTest properties). No nested rows, no host buses,
no network, no hardware.

| Command | Result |
| --- | --- |
| `cmake --build <ROOT>/debug --parallel 3 --target qindaqt_shell_icons qindaqt_shell_icons_locator_tests qindaqt_shell_icons_resolver_tests qindaqt_shell_icons_provider_tests qindaqt_shell_icons_qml_tests` | exit 0, strict warnings, zero warnings |
| same for `<ROOT>/release` | exit 0, zero warnings |
| `ctest --test-dir <ROOT>/debug -R '^qindaqt\.shell-icons-' --output-on-failure --no-tests=error` | **4/4 passed**, exit 0 |
| `ctest --test-dir <ROOT>/release -R '^qindaqt\.shell-icons-' …` | **4/4 passed**, exit 0 |
| Per-binary totals (Debug and Release identical) | locator 30/30, resolver 10/10, provider 24/24, qml 3/3, 0 failed/skipped |
| `./tools/validate-docs` | "Validated 145 Markdown documents and mkdocs.yml navigation", exit 0 |
| `mkdocs build --strict --site-dir <ROOT>/site` (docs venv) | exit 0 |
| `./tools/check-source-shape` | exit 0 (only pre-existing other-lane review-threshold warnings; none in owned paths) |
| `git diff --check` | exit 0 |
| `python3 -m json.tool` on changed JSON | not applicable — no JSON changed |
| `attack-old/rss_flood_control` (unrepaired sources) | delta 329,272 KiB, exit 1 (control fails as required) |
| `attack-old/hicolor_cap_control` (unrepaired sources) | hicolor lost, exit 1 (control fails as required) |

## Remaining bounded caveats

- This candidate deliberately does not change the recolor behavior for
  semi-transparent colors (P3-2 is repaired as a documented rule, per the
  brief's "minimally"); an alpha-carrying `Icon.color` still renders the
  symbolic icon un-recolored.
- `cacheEntryCount()`/`cacheKeyBytes()` are new public observability seams
  for the module's own tests; existing signatures are unchanged, so the
  shell-icon-first-applets consumer is unaffected.
- RSS assertions use a generous 64 MiB ceiling after `malloc_trim`; the
  primary control is the exact cache entry/key-byte assertions.
- No nested-session, windowed, or hardware claims; all rows offscreen with
  host display/bus variables unset.

## Requested next action

Independent exact review by Carolyn Bertozzi (recheck once), then manager
integration.
