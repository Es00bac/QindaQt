# Gideon Fox — Font F0 exact-candidate review: FAIL (0/1/0/3)

- Time: 2026-08-28T19:00:44Z
- Reviewer: Gideon Fox (Font F0 exact-candidate reviewer; Anthropic Claude Code
  `claude-sonnet-5`, reasoning: high)
- Candidate: `9575e2375f5c9c5aeea9d5a90a0a0f185fd96f66` (Faye Lin, Google
  Antigravity Vertex ADC `gemini-3.7-flash-high`)
- Claimed base: `146fc48358c2659436dec4fc6b6062d23c5ee746`
- Review worktree: `/mnt/d/QindaQt/worktrees/font-f0-review-gideon` (detached
  at the exact candidate, read-only for the entire review)
- Verdict: **FAIL** — P0/P1/P2/P3 = **0/1/0/3**
- Related: [Faye's handoff](1787943000-faye-lin-font-f0-handoff.md),
  [Faye's takeover claim](1787941088-faye-lin-font-f0-takeover-claim.md)

## Provenance and cleanliness

- `git log -1 --format="%H %P" 9575e237...` confirms a single parent
  `146fc483...`, matching the claimed base exactly.
- Content diff against base is exactly the 34 paths listed in Faye's handoff;
  no unowned paths touched (`features.json`, `docs/TASK_LIST.md`,
  `docs/HANDOFF.md`, Settings app, Shell untouched).
- `git status` in the review worktree shows ~1,339 files as modified, but
  every one is a `100644 -> 100755` file-mode flip with 0 insertions/0
  deletions (`git diff --stat` confirms `0 insertions(+), 0 deletions(-)`) —
  this is a `/mnt/d` DrvFS/WSL mount artifact with `core.fileMode=true`, not a
  candidate defect. No content drift.
- Scanned every other active worktree's HEAD tree for
  `docs/wiki/adr/0042-pure-font-catalog-and-preference-boundary.md`: no
  collision. ADR-0042 is correctly the next number after ADR-0041 in the base
  index; `mkdocs.yml` nav and `docs/wiki/adr/index.md` both updated
  consistently.

## Build and test evidence (independently reproduced)

- Configured and built strictly under `/mnt/d` with
  `-DCMAKE_AUTOMOC_PATH_PREFIX=ON`, `QINDAQT_ENABLE_STRICT_WARNINGS=ON`, both
  `CMAKE_BUILD_TYPE=Debug` and `Release`. Zero compiler warnings in either
  build log.
- `ctest -L fonts` in both `build/dev` and `build/release`: **7/7 passed**
  (catalog, preferences, codec, bootstrap, coordinator, boundary,
  installed-consumer), matching the handoff's claimed totals exactly.
- `tools/check-source-shape`: 1,155 source files, 0 limit violations.
- `tools/validate-docs`: 76 Markdown documents + `mkdocs.yml` nav validated
  cleanly. (`mkdocs` itself is not installed in this environment, so
  `mkdocs build --strict` could not be run directly; the repo's own
  `validate-docs` tool is the available substitute and passed clean — noting
  this as unavailable coverage per AGENTS.md, not a candidate defect.)
- `git diff --check` on the candidate commit: clean, zero whitespace
  violations.
- Adjacent regression check: rebuilt and reran
  `qindaqt.design-tokens-derivation` and `qindaqt.settings-schema` — both pass
  unaffected (this candidate only adds one line each to `src/CMakeLists.txt`
  and `tests/CMakeLists.txt`, correctly placed and additive).
- Independently confirmed the pure-boundary claim: `font_preferences/**`
  sources use only Qt Core/Gui value types (`QString`, `QFont`, `QJsonObject`,
  `QVariantMap`); no `QtDBus`, `QtQml`, `QtQuick`, `fontconfig/`, `QProcess`,
  or `QThread` — `check_boundary.cmake` and manual source read agree.
- Independently confirmed the LKG/atomic contract:
  `FontPreferencesCoordinator::refreshCatalog`/`updatePreferences` only
  mutate state and advance `m_revision` on success; failure paths leave
  catalog/preferences/LKG/revision untouched. Verified in source and by the
  coordinator's hostile-input tests.
- Independently confirmed the installed-consumer test: it stages public
  headers to an isolated prefix, configures/builds a standalone external
  consumer against the installed `qindaqt_font_preferences` static library,
  and runs it — a real out-of-tree link, not a source include.

## P1 — false Settings1 schema-key compatibility claim (blocking)

`src/services/font_preferences/include/qindaqt/services/font_preferences/font_validation.h:9-10`
defines `fonts.pointSize` validity as `[MinPointSize=4.0, MaxPointSize=144.0]`.
The **live, enforced** Settings1 schema declares a materially tighter range
for the identical key:

- `data/settings/schema-v1.json:58-64` and `data/settings/schema-v2.json:79-85`
  — `"fonts.pointSize"`, `"constraints": {"minimum": 6.0, "maximum": 36.0}`.
- This is not a dead/aspirational schema: `src/settings/src/settings_value_normalizer.cpp:125-132`
  reads `definition.minimum`/`definition.maximum` for `Number`-typed keys and
  rejects out-of-range values (`"value is above maximum %1"`), and I rebuilt
  and reran `qindaqt.settings-schema` to confirm this path is live and
  passing today.

`font_preferences_codec.h:29-31` states the codec's settings keys "match the
schema-v2 `fonts.*` keys," and ADR-0042 claims "Lossless Codecs ... Settings1
schema keys" compatibility. Both claims are false for this key's numeric
bounds: `FontPreferencesCodec::fromSettingsMap` and
`FontPreferencesCoordinator::updateFromSettings` will accept, e.g.,
`{"fonts.pointSize": 100.0}` as valid (revision advances, LKG updates) even
though the real `SettingsSchema::validateValue("fonts.pointSize", 100.0)`
rejects it. No test in `tests/services/font_preferences/tst_font_preferences_codec.cpp`
or `tst_font_preferences_coordinator.cpp` exercises the 36–144 gap — coverage
only probes `11.0`, `9999.0`, and the module's own `4.0`/`144.0` bounds.

This will surface as a silent round-trip inconsistency the moment Font F0 is
wired into the real Settings1 persistence path — explicitly the next roadmap
step named in ADR-0042's "Revisit when" section. Repair: either tighten
`MinPointSize`/`MaxPointSize` (or add a schema-aware validation path) to match
`[6.0, 36.0]`, or correct the codec/ADR documentation to stop claiming exact
schema-key compatibility and add a boundary test covering the gap.

## P3s (non-blocking, worth fixing alongside the repair)

1. **No NaN/±Infinity test coverage.** `isValidPointSize`/`clampPointSize`
   and `isValidLogicalDpi`/`clampLogicalDpi`
   (`src/services/font_preferences/src/font_validation.cpp:9-33`) correctly
   guard with `std::isfinite` on inspection, but nothing in
   `tst_font_preferences.cpp` or `tst_font_preferences_codec.cpp` exercises
   `NaN`/`Infinity` inputs. AGENTS.md requires round-trip/invariant tests for
   model mutations; this invariant is implemented but unproven.
2. **Implicit empty-result invariant.** `FontPreferencesCoordinator::refreshCatalog`
   (`font_preferences_coordinator.cpp:17-37`) treats `FontCatalog::create(...).isEmpty()`
   as its sole failure signal. This is correct today only because
   `FontCatalog::create` happens to never produce zero entries from a
   non-empty, fully-valid fact list — an implicit coupling rather than an
   explicit `std::optional<FontCatalog>`/success flag. Low risk, but fragile
   to a future refactor of `FontCatalog::create`.
3. **Lossy hinting collapse undocumented.** `toQtHinting`
   (`font_bootstrap.cpp:8-21`) maps both `FontHinting::Medium` and
   `FontHinting::Full` to `QFont::PreferFullHinting` — an inherent limit of
   Qt's `HintingPreference` enum (Qt has no "medium" value), not a bug, but
   worth one contract-comment line so a future reader doesn't mistake it for
   an oversight.

## Requested next action

Return to Faye Lin for a non-amended repair: correct the `fonts.pointSize`
bound mismatch (P1) and, ideally, add the NaN/Infinity and 36–144 boundary
tests (P3-1). I remain the assigned independent reviewer for the exact
rereview. Candidate commit, worktree, and this verdict are preserved
unedited.
