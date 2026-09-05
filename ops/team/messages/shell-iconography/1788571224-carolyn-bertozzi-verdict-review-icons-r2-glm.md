# Exact-candidate review — Shell iconography I1, funded recheck

- **Reviewer:** Carolyn Bertozzi, independent shell reviewer (slug `carolyn-bertozzi`)
- **Provider/model:** Z.AI GLM, `zai-coding-plan/glm-5.3`
- **Candidate SHA:** `288574a8fcc673859bbbf12e1f1c2b38c8c6ebdc` (`worker/shell-icons`, repair by Gerty Cori, Moonshot Kimi)
- **Candidate tree SHA:** `d5d19409b1b227d6a1f6dd7535059e99a123afea` (verified `git rev-parse HEAD^{tree}`)
- **Parent SHA:** `437d3b54d51ecf9dab11ce8b185f858da858f5ab`
- **Base SHA:** `37f8523ed105e66d9784f8cca767d060eed3f1da`
- **Review worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/shell-icons-glm-review` (detached HEAD at the candidate; `git status --porcelain` empty before and after review; no product file edited, committed, staged, or reverted)
- **Build root:** `/home/cabewse/work_SPaC3/builds/qindaqt/review-icons-glm` (`<ROOT>` below, reused from the rejected round; system-KWin initial cache)
- **Date:** 2026-09-04
- **Addresses:** my REJECT 0/1/0/4 on `54cda1fb`
  (`/home/cabewse/work_SPaC3/builds/qindaqt/lanes/review-icons-glm/verdict.md`); this is the one funded recheck.

## Findings ledger

### P0

None.

### P1

None. **P1-1 (unbounded byte retention of hostile provider ids) is closed.**
The repair is cause-level and matches the handoff:

- `src/shell/icons/src/icon_image_provider.cpp:70-76` refuses any id over
  `kMaxRequestIdUtf8Bytes` (1024, new documented constant in
  `icon_theme_limits.h:31-34`) before `parseRequest` and before any cache
  access; refused ids return the deterministic placeholder with a non-null
  `*size`.
- `cacheKeyFor` (`icon_image_provider.cpp:135-151`) keys the LRU on the
  parsed, bounded request tuple. I verified the encoding is injective: the
  `\x1f` separator cannot occur in a grammar name (`[A-Za-z0-9._-]`), in the
  integer size, in the 17-significant-digit scale serialization
  (round-trip-exact for doubles), in the `0`/`1` symbolic flag, or in the
  `#rrggbb`/empty color field — so distinct tuples never collide and
  identical tuples always share. The invalid-request sentinel `"\x1f"`
  cannot collide with any valid key. Key bytes are bounded by construction
  (≤ ~180 bytes/entry; 64 entries).

### P2

None.

### P3

**P3-1 No QML-level negative control for the new `effectiveName` grammar bound.**
`src/shell/icons/qml/Icon.qml:32-39` and the wiki `name` row
(`docs/wiki/shell/iconography.md:114`) claim names outside the bounded
grammar "never reach the URL layer", but `tests/shell/icons/tst_shell_icons_qml.cpp`
contains only valid-name rows; a tree with the bound deleted (returning
`root.name`) still passes the whole product suite. The fail-closed *outcome*
is independently covered at the enforcing layer (provider
`hostileIdsReturnPlaceholder`, `hostileNamesRefused`, over-long/flood rows),
and I verified the element behavior live with a scratch probe (below), so
this is coverage precision on a defense-in-depth seam, not a live defect.
Reproduction of the gap: delete the grammar check in `effectiveName`, rerun
`ctest -R '^qindaqt\.shell-icons-'` — still 4/4; my probe rows Q2/Q5/Q7
(hostile `name` leaves the inner Image `source` empty) would fail.

**P3-2 The "cap is never exceeded" half of the hicolor boundary is code-verified only.**
`tests/shell/icons/tst_shell_icons_locator.cpp:149-162` proves hicolor
survives a flooded chain but would also pass on a tree that appends hicolor
*without* removing the deepest entry (17-entry chain): `locate("fallback")`
resolves identically. Code reading confirms the cap holds
(`icon_theme_locator.cpp:161` caps expansion at 16; `:149-154` removes the
deepest entry before appending hicolor), and the hicolor-survives half does
fail on `54cda1fb` (verified live). A fixture icon present only in
`flooded-15` — the theme that must yield — would pin the other half.
Nonblocking.

Non-finding observations (no severity): the over-length check's
`id.toUtf8()` (`icon_image_provider.cpp:70`) makes a transient copy
proportional to the caller's id before refusing — caller-bounded, released
immediately, and my RSS harness proves nothing is retained; acceptable. The
QML `length > 128` check counts UTF-16 units where C++ counts UTF-8 bytes,
but the admitting regex is ASCII-only, so the two bounds coincide for every
name that passes.

## Repair diff review (54cda1fb → candidate, product surface)

- `icon_image_provider.{h,cpp}`: request ceiling + tuple keying + the two
  const, mutex-guarded observability seams (`cacheEntryCount`,
  `cacheKeyBytes`; documented "Product code must not branch on these").
  Public API additions only; no existing signature changed.
- `icon_theme_locator.{h,cpp}`: hicolor yields-at-cap repair, coherent with
  the expansion-side cap in `expandTheme`; header updated.
- `icon_theme_limits.h`: `kMaxSvgSourceBytes` (P3-4) and
  `kMaxRequestIdUtf8Bytes`, each documented with its consumer.
- `icon_runtime.h`: P3-1 comment now states refuse-and-change-nothing,
  matching `icon_runtime.cpp:19-21`.
- `Icon.qml`: `effectiveName` grammar mirror; `resolved`, URL construction,
  and `Accessible.name` all route through it.
- `P3-2` is doc-only (QML comment, wiki `color` row, provider section):
  alpha-carrying colors are documented as non-targets; behavior unchanged,
  as the handoff's caveat states.
- ADR renumber 0071 → 0072 is complete in-tree: file renamed, `adr/index.md`
  row, `mkdocs.yml` nav, and every reference in `iconography.md`,
  `testing-harness.md`, `module-boundaries.md`; `grep -rn "0071-shell-iconography\|ADR-0071"`
  over docs/mkdocs/src/tests finds nothing. The 0071 gap matches the index's
  never-reuse/reservation policy; main's 0071 occupancy is per the lane brief
  (my local `origin/main` ref is stale and was not fetched — no network).
- Shared-registry edits are single-purpose renumber lines; module CMake and
  linkage are untouched by the repair (`QindaQt::ShellLauncher` +
  Qt Core/Gui/Qml/Quick/Svg only).

## Review surface covered

1. **Question 0 — P1-1 recheck.** My original harness re-run against the
   candidate (Debug and Release): 70 distinct 4 MiB hostile ids, caller
   release + `malloc_trim(0)`, `/proc/self/status` VmRSS — retained delta
   **1,052 KiB**, eviction flood causality check clean, **exit 0** both
   profiles (was 513.9 MiB / exit 1 on `54cda1fb`). Negative controls on
   the unrepaired sources (extracted via `git archive 54cda1fb` into
   `<ROOT>/attack-old/`, never inside the worktree): the product RSS-row
   logic (40 × 4 MiB) retains **329,276 KiB**, exit 1; the hicolor-cap row
   loses the fallback, exit 1. The same two controls on the candidate
   sources: 1,048 KiB and hicolor holds, both exit 0. All five new product
   rows exist, run (0 skipped), and pass.
2. **Four P3s + ADR.** Verified as itemized above.
3. **IconThemeLocator re-attack.** My 24-check hostile-fixture harness
   (spec-realistic slashed `48x48/apps` subdirectories, CRLF indexes,
   symlinked theme directories, dangling symlink icons, inverted
   MinSize/MaxSize, hostile `Inherits` parents, oversized injected names,
   duplicate hicolor injection, dotted/leading-dot/`..` grammar edges,
   scale-2-over-1 device distance; resolver traversal/absolute/NUL/huge
   ids, empty/spaced `Icon=`): 0 failures on the candidate.
4. **Provider cache re-attack (new, 21 checks, Debug and Release, under
   `QT_FATAL_WARNINGS=1`).** Sentinel collapse of 240 distinct invalid
   spellings to one entry/one key byte; the exact 1024/1025-byte boundary
   (served-with-sentinel vs refused-with-no-cache-touch); tuple sharing
   across parameter order and format spellings with byte-identical images;
   distinct size/color/symbolic tuples never sharing entries or pixels
   (red vs green recolor pixel-exact); entry and key-byte bounds under a
   200-request mixed churn with results still correct afterwards.
5. **QML `Icon` element re-attack (new, 9 checks, compiled module through
   the real `IconRuntime::install` seam with published QST-1 tokens).**
   `../secret`, a 129-char name, and a spaced name: `resolved` false, inner
   Image `source` empty (never reaches the URL layer), accessible name
   empty rather than exposing hostile bytes; a valid name still builds
   exactly `image://qindaqt-icon/exact?size=16&scale=1`. Pure-Qt probe
   re-confirmed: Qt delivers the URL query verbatim to `requestImage`.
6. **Tests read line-by-line.** New rows are real (each would fail on the
   unrepaired tree — verified live for the two behavioral ones, established
   by the observability seams for the count/byte ones). No vacuous or
   tautological rows found. The two coverage-precision gaps are P3-1/P3-2
   above.
7. **Boundaries/shape.** No host lookups (`QIcon::fromTheme`,
   `QStandardPaths`, `QDir::home`, `getenv`) and no filesystem writes in
   `src/shell/icons`; production files ≤ 423 non-blank lines; no
   nested/windowed/hardware/network/bus contact anywhere in this review.

## Commands and results (all actually run)

Environment for every test/attack run: `DISPLAY`/`WAYLAND_DISPLAY`/
`DBUS_SESSION_BUS_ADDRESS` unset and
`DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent`; offscreen rows
additionally `QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software
QT_FATAL_WARNINGS=1`. No nested-compositor rows, host buses, hardware, or
network; all scratch harnesses and extracted old sources under
`<ROOT>/attack` and `<ROOT>/attack-old`.

| Command | Result |
| --- | --- |
| `git rev-parse HEAD` | `288574a8fcc673859bbbf12e1f1c2b38c8c6ebdc` (matches candidate) |
| `git status --porcelain` (before and after review) | empty, exit 0 |
| `cmake --build <ROOT>/debug --parallel 3 --target qindaqt_shell_icons qindaqt_shell_icons_locator_tests qindaqt_shell_icons_resolver_tests qindaqt_shell_icons_provider_tests qindaqt_shell_icons_qml_tests` | exit 0 (23/23 steps, strict warnings, zero warnings) |
| same for `<ROOT>/release` | exit 0 (23/23 steps, zero warnings) |
| `ctest --test-dir <ROOT>/debug -R '^qindaqt\.shell-icons-' --output-on-failure --no-tests=error` | **4/4 passed**, exit 0 |
| `ctest --test-dir <ROOT>/release -R '^qindaqt\.shell-icons-' …` | **4/4 passed**, exit 0 |
| Per-binary totals (Debug and Release identical) | locator 30/30, resolver 10/10, provider 24/24, qml 3/3, 0 failed/skipped (final clean re-run of both selectors confirmed 4/4 again) |
| `./tools/validate-docs` | "Validated 145 Markdown documents and mkdocs.yml navigation", exit 0 |
| `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir <ROOT>/site` | built in 2.07 s, exit 0 |
| `./tools/check-source-shape` | exit 0 (warnings only in other lanes' files; none under `src/shell/icons` or `tests/shell/icons`) |
| `git diff --check` | exit 0 |
| `python3 -m json.tool` on changed JSON | not applicable — the 54cda1fb→candidate diff changes no JSON files |
| `attack_provider_mem` (my P1-1 reproduction, Debug + Release) | retained delta 1,052 KiB / 1,052 KiB, exit 0 both |
| `attack-old/rss_flood_control` (54cda1fb sources) | delta 329,276 KiB, **exit 1** (control fails as required) |
| `attack-old/rss_flood_control` (candidate sources) | delta 1,048 KiB, exit 0 |
| `attack-old/hicolor_cap_control` (54cda1fb sources) | hicolor lost, **exit 1** |
| `attack-old/hicolor_cap_control` (candidate sources, two fixture roots) | hicolor holds, exit 0 |
| `attack_locator` (24 hostile checks, candidate) | 0 failures, exit 0 |
| `attack_cache` (21 cache-keying checks, Debug + Release, fatal warnings) | 0 failures, exit 0 both |
| `attack_icon_grammar` (9 compiled-module QML checks) | 0 failures, exit 0 |
| `attack_qml` (pure-Qt verbatim-query probe) | query delivered verbatim, exit 0 |

## Verdict

The repair is the one I asked for and it is done at the cause: ids are
length-refused before any parsing or cache access, and the LRU keys on the
parsed, bounded request tuple with an injective encoding and a
non-colliding sentinel. My original 513.9 MiB reproduction now retains
1.0 MiB in both build profiles; the negative controls fail on the
unrepaired tree exactly as the handoff claimed (329,276 KiB; hicolor
lost), and my new attacks against the keying — sentinel collapse, boundary
ids, spelling-share and tuple-distinctness, collision resistance, churn
bounds — found nothing. All four P3s are repaired or accurately
documented, the ADR renumbering is complete in-tree, the focused suites
are 4/4 with matching subtest counts in Debug and Release, and every
static gate is green. What remains are two coverage-precision gaps at
defense-in-depth seams whose enforcing layers are fully tested, and I
verified both behaviors live myself.

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/2
