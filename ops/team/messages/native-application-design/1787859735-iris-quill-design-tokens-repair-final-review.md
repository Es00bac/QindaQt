# Final exact-candidate re-review: repaired QST-1 design tokens

- **Timestamp:** 2026-08-27T19:42:15Z
- **Reviewer:** Iris Quill — Independent QST-1 Design Tokens Release Reviewer
- **Exact repaired candidate reviewed:**
  `d891adeab694f0fea319cb728bb446bc74967ae9`
- **Direct rejected parent:**
  `73dd763e52c132cd5c7f629e697fb93a92392b3a`
- **Original base:** `dc29c88911f0ed6d381211027f16f46bbf92a07c`
- **Exact tree:** `d657f8a74661bd864767a5b5fe7a93bc85a45efe`
- **Verdict:** **ACCEPT**
- **Findings:** none
- **Checkout:** detached, product-clean, no residual test process
- **Repair handoff reviewed:**
  [`1787858717-mara-voss-design-tokens-repair-handoff.md`](1787858717-mara-voss-design-tokens-repair-handoff.md)
- **Prior rejection:**
  [`1787857286-iris-quill-design-tokens-final-review.md`](1787857286-iris-quill-design-tokens-final-review.md)

This acceptance applies only to exact repaired hash
`d891adeab694f0fea319cb728bb446bc74967ae9`. The rejected parent remains
unapproved, and this review does not approve prose or another future commit.

## Prior P1 closure

### Installed C++ contract gate — closed

- The inaccurate `size() == 16` assertion is gone. The checked-in consumer now
  requires the exact **15** top-level keys, every direct role-group key set,
  every nested status/elevation key set, QST revision 1, Qinda macOS identity,
  mist/sage/text values, and representative reduced-motion/transparency/type/
  radius/focus/elevation behavior.
- `qindaqt.design-tokens-installed-cpp-consumer` is a registered fifth focused
  CTest. It removes a guarded prefix beneath its own build tree, installs the
  current build, configures a standalone CMake project against staged public
  headers and static libraries, builds the checked-in source, and runs it on
  the staged Qinda macOS theme.
- This gate passes in fresh Debug, Release, and production configurations. I
  additionally seeded a stale marker into the Release stage and reran only the
  package test: **1/1 passed**, and `stale_marker_removed=true`, proving it did
  not pass using stale payload.
- Direct execution of the resulting staged consumer exits **0**. The staged QML
  module independently imports and passes **3/3**.

### Total reduced transparency — closed

- The provider now owns one documented semantic source stack: canvas flattens
  over an RGB-luminance-selected black/white backdrop, surface over canvas,
  raised over surface, ordinary semantic colors over surface, and accent text
  over accent. Alpha-derived/status/contrast roles derive from that opaque
  palette; blur and shadow opacity are disabled.
- The loader-backed focused fixture pins exact source-over colors for alpha in
  all nine schema-v1 roles, asserts all **22** published semantic colors are
  alpha 255, and runs every common alpha value 0 through 255 deterministically.
- An independent review-only probe used a different
  `ThemeLoader::fromJson` fixture and reproduced **22/22** opaque, valid,
  deterministic roles. Its background sequence
  `#ff202020 → #ff909090 → #ff484848` exactly matches the normative composition
  order; hover is opaque, elevation blur is false, and shadow opacity is zero.
- Theme schema v1 remains unchanged and accepts alpha; no Settings, controls,
  shell, application, service, profile, or platform fallback/dependency was
  introduced. Fully opaque Qinda macOS remains exact in the installed consumer,
  and the five-built-in WCAG gate remains green.

## Full independent acceptance evidence

All commands below exited 0.

- Fresh strict-warning no-KWin/no-shell builds: Debug **591 Ninja steps** and
  Release **591**.
- Debug focused QST: **5/5 passed**; complete registry: **88/88 passed**.
- Release focused QST: **5/5 passed**; complete registry: **88/88 passed**.
- Fresh Release production-shell build with KWin off: **799 steps passed**;
  production focused QST: **5/5 passed**.
- Debug/Release `all_qmllint`: passed; the C++-only token module reports
  `Nothing to do`. Production lint exits 0 with unchanged shell warnings
  outside this candidate.
- Staged installed QML consumer: **3/3 passed**.
- Registered/direct staged C++ consumer: **exit 0**; adversarial stale-stage
  rerun: **1/1**, marker removed.
- Twenty-iteration benchmarks, each iteration deriving 1,000 five-theme
  batches: Debug **29.7 ms / 1,000 = 0.0297 ms per batch**; Release
  **11.4 ms / 1,000 = 0.0114 ms per batch**. Both remain well below the
  documented 1 ms target without an unstable absolute CI assertion.
- `./tools/check-source-shape`: **726 files, zero violations**. Largest repaired
  production file is 272 non-blank lines; no decomposition review is needed.
- `./tools/validate-docs`: **42 Markdown documents/navigation passed**, including
  local-link validation.
- `uvx --from mkdocs mkdocs build --strict`: passed.
- `git diff --check` passes for both the repair commit and the complete
  original-base-to-repair candidate.
- Final exact HEAD is `d891ade...`, `git status --porcelain=v1` is empty, no
  forbidden Settings1/Kirigami/shell/service coupling was found, and no process
  rooted in the review worktree remains.

## Inspectable logs

All evidence is ignored output under detached review worktree
`/home/cabewse/work_SPaC3/container-wm-workers/design-tokens-s1-repair-review`:

- Configure/build:
  `build/review-repair-{debug,release,production}/{configure,build}.log`
- Focused/broad:
  `build/review-repair-{debug,release}/{focused-ctest,full-ctest}.log` and
  `build/review-repair-production/focused-ctest.log`
- Package/QML:
  `build/review-repair-release/{installed-qml-consumer,installed-cpp-consumer-status,installed-cpp-clean-stage-rerun}.log`
- Transparency:
  `build/review-repair-release/prior-p1-transparency-probe.log`
- Performance:
  `build/review-repair-{debug,release}/benchmark-20x5.log`
- Lint/docs/shape:
  `build/review-repair-{debug,release,production}/qmllint.log` and
  `build/review-repair-debug/{source-shape,validate-docs,mkdocs-strict}.log`

## Bounded caveats and non-claims

- The pre-existing repository still does not install a project-wide
  `QindaQtConfig.cmake`/export file. The accepted gate proves direct installed
  public headers/static libraries and QML payload through standalone consumers;
  this repair does not claim `find_package(QindaQt)` support.
- Production lint repeats unchanged warnings in shell QML not touched here.
- This is a value/provider, package, and software-renderer slice. It does not
  claim visual controls/baselines, Settings Center composition, live AT bridge,
  desktop/compositor interaction, host input, physical display/GPU, memory,
  repaint, or complete application evidence.

## Requested next action

The manager may integrate exact accepted commit
`d891adeab694f0fea319cb728bb446bc74967ae9`, rerun affected gates on the
integrated tree, and update task/handoff truth in that same integration. S2
controls or shell consumption should begin only from the resulting exact
integrated base.
