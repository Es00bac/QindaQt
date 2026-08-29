# Arden Pike — C0 recovery byte audit result and repair plan

- Time: 2026-08-28T20:07:00Z
- Subject: byte-for-byte audit of the inherited dirty tree at base
  `146fc48358c2659436dec4fc6b6062d23c5ee746`

## Preserved as useful (no functional change needed)

- `src/CMakeLists.txt` / `tests/CMakeLists.txt`: exactly one additive
  `add_subdirectory(services/display_color_model)` line each, correct order.
- Module `CMakeLists.txt`: byte-pattern of `brightness_model`'s accepted
  registry (static lib, header file set, export, install).
- `color_limits.h`, `color_types.h`, `color_validation.h`: sound bounded value
  model; kept.
- `installed_consumer/` (C++ consumer and its CMakeLists): kept.
- All three QtTest suites: structure and fixtures kept; extended, not replaced.
- Local-only `ops/team/` coordination bytes in the worktree: preserved on disk
  as session state, never committed (Solene's shared-board profile already
  reconciles the identity).

## Rejected defects (with reasons)

1. `check_boundary.cmake` and `run_installed_cpp_consumer.cmake` open with C++
   `//` comment syntax; CMake only accepts `#`, so both registered tests are
   parse-fatal today. Repair: `#` SPDX lines.
2. `ColorModel::validateLineage()` returns `revision >= m_revision`, accepting
   future/out-of-order publications, and `tst_color_model.cpp` pins that wrong
   expectation. This violates Solene's own claimed contract ("rejection of
   stale or out-of-order publications") and Display1's exact epoch/revision
   equality rule. Repair: exact equality + corrected/extended tests.
3. `color_validation.cpp` uses `QSet` without including it (compile-fatal) and
   carries two unused includes (`QDataStream`, `QRegularExpression`).
4. Hostile-input gaps: no NaN/infinity luminance checks, no enum-range checks
   for cast origin/gamut/transfer/policy/intent values, no `MaxOutputs` (32)
   aggregate cap on `updateCapabilities`/`requestAssignment`, no bound that
   `rawHeader`/`headerData` cannot exceed the declared `byteSize`, no ICC
   version-field bounds. All get fail-closed repairs with regressions.
5. When HDR/WCG policy is clamped to SDR, the applied assignment still kept the
   requested HDR/WCG profile. Repair: policy clamp falls back to the default
   sRGB profile so applied truth is coherent (requested stays truthful).

## Missing and being finished

Wiki page `architecture/display-color-model.md`, ADR-0030 (as reserved by
Solene's claim), mkdocs/nav/index/module-boundaries/ADR-index additive rows,
a poison-negative proof for the source-policy row, hostile aggregate/atomic
no-change-on-reject tests, Debug+Release focused runs, source-shape, docs
validation, strict MkDocs, then one clean candidate commit crediting Solene
Ward authorship.
