# Tessa Rowan — Controls S2 exact-candidate verdict: FAIL

Timestamp: 2026-08-27T21:40:41-06:00

## Exact identity and verdict

- Candidate: `10996f146ff78f69a6f1019933d812d1475faf85`
- Tree: `ed48f540b36f8d2d7f1f865d4493d02c74f9daf0`
- Sole parent/base: `a083a20af14a2d7b9e954735a2d659c475a536b2`
- Detached review worktree: clean, exact, no branch
- Diff: 63 paths; no production shell path
- Verdict: **FAIL — P0/P1/P2/P3 = 0/0/2/3**

The two P2 defects below require the original implementer, Cora Vale, to create
one new non-amended descendant and return that exact commit/tree for this same
reviewer. Prose, a mutable worktree, or rerunning the old commit cannot close
them.

## Blocking P2-1 — incomplete installed QML/tooling payload

Exact reproduction from source and preserved stage:

1. `src/controls/CMakeLists.txt:14-28` declares 14 public QML documents.
2. Its install rules at `:55-67` deploy the two libraries, `qmldir`, and
   `qindaqt_controls.qmltypes`, but none of those QML documents.
3. The clean Release stage contains exactly those four files. Its installed
   `qmldir` advertises `qml/Button.qml` through `qml/TokenSwatch.qml`; none
   exists on disk. Its typeinfo is only `Module {}` because the public types
   are QML-defined rather than plugin-defined.
4. `tests/controls/run_installed_controls_consumer.cmake:37-66` requires only
   metadata and runs qmltestrunner. Runtime succeeds through the module's
   compiled `prefer :/qt/qml/QindaQt/Controls/` resources, so this gate cannot
   establish installed `qmllint`, cache-generation, IDE/type inspection, or a
   separately built first-party QML consumer.

This contradicts the reusable installed module outcome. Qt's official
`qt_query_qml_module` deployment contract exposes `QML_FILES` and
`QML_FILES_DEPLOY_PATHS` specifically to deploy every module part:
https://doc.qt.io/qt-6/qt-query-qml-module.html#description

Required closure:

- install all 14 QML documents at the generated deploy paths (use queried
  paths rather than duplicating a second manual inventory);
- make the clean stage require the exact 14-file inventory; and
- add a clean staged tooling/build consumer that analyzes representative
  public properties with source/build/ambient QML paths absent, while
  preserving the existing runtime qmltestrunner and `$ORIGIN/../Tokens`
  relocation proof.

Full finding: `1787888115-tessa-rowan-controls-installed-tooling-finding.md`.

## Blocking P2-2 — StateCard can announce silence or stale content

Exact source reproduction:

1. The promised announcement tuple is status + title + message at
   `src/controls/qml/StateCard.qml:42-68`.
2. Only `onStatusChanged` invokes it at `:71`.
3. Changing title/message while Warning or Error remains active is therefore
   silent. Publishing status before new title/message announces the old tuple
   synchronously, and the later content changes never correct it.
4. `tests/controls/tst_controls_behavior.cpp:324-378` changes only status with
   constant content, so its “dynamic” test cannot detect either failure.
5. `StateCard.qml:25-26` exposes writable `accessibilityReady` and
   `accessibilityRevision` test/implementation state as accidental public 1.0
   authority; setting readiness false suppresses every announcement.

Required closure:

- keep readiness/revision private to the implementation (signal counts already
  prove publication);
- schedule/coalesce post-construction status/title/message changes so one event
  turn emits exactly one complete latest tuple with correct politeness; and
- add deterministic same-status message-update and status-then-new-content
  regressions that reject silence, stale/intermediate tuples, and duplicates;
  document the coalescing/lifetime rule.

Full finding: `1787888307-tessa-rowan-controls-s2-review-midpoint.md`.

## Nonblocking P3 corrections for the same repair touch

1. `tests/controls/run_controls_visual_row.cmake:38-42` has a stale
   `AGENT-GUARD` asserting process-global glyph/geometry retention as proven
   cause. The later evidence and ADR-0021 correctly say process isolation did
   not settle pixels and the missing sufficient boundary was QST motion. Keep
   one-process-per-row for exact row lifetime/hygiene, but rewrite the marker
   to the accepted evidence rather than the superseded causal claim.
2. `pinDeterministicFonts()` substitutes two family names to whatever host
   Noto Sans/Noto Sans Mono is installed and asserts only Noto Sans exists
   (`control_test_support.cpp:107-116`, `tst_controls_visual.cpp:64-69`). It
   does not pin font bytes/version as `controls.md:118` and the harness imply.
   Either load/hash repository-controlled font assets or qualify the prose as
   a named test-environment substitution and explicitly require both families.
3. `check_control_source_policy.cmake:13-34` blocks five named imports but does
   not enforce the documented allowlist. The current 14 files are clean, so
   this is not a candidate dependency violation; harden the guard to permit
   only QtQuick, QtQuick.Controls, QtQuick.Layouts, and QindaQt.Tokens rather
   than attempting to enumerate future services/frameworks.

## Every other assigned surface is closed

- All 14 production QML files obey the current token-only dependency boundary;
  no theme identity, sourceThemeId, palette hex, Settings1, shell, service,
  LayerShellQt, Kirigami, or application import exists. Target linkage remains
  one-directional through Tokens and Qt.
- The public wiki truthfully states normal visual-parent lifetime, GUI-thread
  affinity, visual/accessibility error ownership, compatibility expectations,
  supported caller-owned availability, FormRow editor association, hostile
  ThemeCard totality, DegradedNotice specialization, RTL, localization,
  reduced transforms, and explicit S2 exclusions.
- Native keyboard behavior/focus, busy/disabled suppression, direct accessible
  roles/states, FormRow naming takeover, hostile preview rejection, five-theme
  compact geometry, and RTL switch/slider geometry are coherently implemented
  and covered, apart from the exact StateCard dynamic gap above.
- All 25 CMake visual rows are unique, scale-compatible, one process/one data
  selector, and guarded by DPR/pixel checks plus the QST motion boundary. All
  25 committed hashes exactly match the durable review inventory. I inspected
  five independently assembled contact sheets; every row is complete and
  unclipped. Dimensions are exact for 100%, 125%, and 150%.
- The staged Controls library directly needs Tokens and carries
  `RUNPATH [$ORIGIN/../Tokens]`; the plugin carries `$ORIGIN` for its backing
  library. No host library/import path masks that runtime proof.
- The PSS script reads exact child PIDs, takes five samples per process over
  three matched pairs, reaps children, and reports paired median delta with
  `threshold: null`. Preserved Release output exactly supports the handoff's
  17,023/37,322/20,299 KiB values; no memory-budget pass is inferred.
- Independent read-only checks pass: exact clean checkout and ancestry/tree,
  diff whitespace, 818-file source shape with zero skips and largest changed
  source 496 non-blank lines, 46-document/nav validation, 14-QML/25-PNG
  inventory, and scope audit. CMake/test registry is exactly 29 Controls rows.
- The handoff accurately withholds a broad pass and accurately scopes the
  reproducible `shell.production-surface.1080p` timeout outside Controls. No
  shell source was altered; that defect is not used to excuse either P2.

I did not configure, compile, build, or run CTest because Mira Quill owned the
sole runtime lane. The review used the immutable source/tree, read-only
preserved build/package/log evidence, independent static validators, exact
hash/dimension checks, and local image inspection. Tessa Rowan remains retained
for exact rereview of Cora's repair descendant.
