# Handoff — Audio applet production repair (descendant of rejected `807b1ae`)

- Worker: Mary Allen Wilkes, Audio applet production implementer
- Provider/model: Moonshot Kimi `kimi-code/kimi-for-coding` "K2.7", reasoning high
- Candidate commit: `caaf7d9a15dd7a936eaee2e8ff83332a5c945970`
- Candidate tree SHA: `0eb16617c18eb0a668614fdeb5543297adf5b4ae`
- Exact base (parent): `6484ad8f9e43ca1b7fc40658a9ae26e1c109c9b8` (handoff commit of rejected `807b1ae`; product base of the rejected candidate remains `35f2fa20881437fc3ef9d85ce399dc68e12ed1d3`)
- Branch: `worker/audio-applet-production`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/audio-applet-production`
- Requested next action: **independent exact review by the same reviewer
  (Barbara Simons, Moonshot Kimi `kimi-code/k3-256k`) of the exact new product
  SHA, then manager integration.**

This commit repairs every finding of the REJECT verdict
(`ops/team/messages/shell-audio-applet/1788410102-barbara-simons-807b1ae-review-reject.md`,
P0/P1/P2/P3 = 0/1/1/2). No product path outside the repair was touched.

## Changed paths (sorted)

- `docs/wiki/development/testing-harness.md`
- `docs/wiki/shell/audio-applet.md`
- `src/shell/runtime/shellruntimeapplication.h`
- `tests/shell/audio_applet/check_boundary.cmake`
- `tests/shell/audio_applet/tst_audio_applet_controller.cpp`
- `tests/shell/qml/imports/QindaQt/Shell/AudioApplet/AudioApplet.qml` (new in git; existed as ignored-untracked)
- `tests/shell/qml/imports/QindaQt/Shell/AudioApplet/qmldir` (new in git; existed as ignored-untracked)

## Finding repairs

- **P1-1** — Committed the two stub import files
  (`qmldir` declaring `module QindaQt.Shell.AudioApplet` +
  `AudioApplet.qml` dispatcher double) that the container exclude rule
  `QindaQt/` (since corrected by the Program Manager to `/QindaQt/`) hid from
  the rejected candidate. Whole-tree exclude audit:
  `git status --porcelain --ignored -- tests/ src/ data/ docs/` returns nothing
  ignored; `git status --porcelain` is empty at the candidate.
- **P2-1** — Implemented the `POISON_ROOT` block in
  `check_boundary.cmake`: the three checked files are copied per case into a
  scratch root under `POISON_ROOT`, one poison is appended, and the same
  script (with `AUDIO_APPLET_PURE_POLICY_SKIP_POISON=ON`) must reject each.
  Four independent poisons mirror the Bluetooth pure gate: D-Bus transport
  include (`QtDBus` token), service-internal include
  (`qindaqt/services/audio_service/...`, rejected as non-boundary include —
  reason verified manually), QML include (`QtQml` token), and QObject
  derivation (`production_extra_forbidden`, production header only). The pass
  line reports the true count: `... (3 files and 4 poison rejections)` with
  `POISON_ROOT`, `... (3 files and 0 poison rejections)` without; a sanity
  check requires exactly 4 when the lane runs.
- **P3-1** — Rewrote the AGENT-GUARD at `src/shell/runtime/shellruntimeapplication.h:144`
  to state the real invariant: `~ShellRuntimeApplication()` unconditionally
  calls `resetRuntime()`, which tears the window factory down before the
  applet compositions; declaration order alone would destroy the compositions
  first (they are declared after `m_windowFactory`).
- **P3-2** — `ownerReplacementClearsTruthAndPendingWithoutReplay` now dispatches
  a stale old-owner `snapshotReply` (old owner `:1.42`, old request id) after
  the replacement and asserts `phaseText() == "loading"` and empty rows before
  the new-owner snapshot is delivered, making the comment's claim executable.
  `AudioClient::acceptSnapshotReply` drops old-owner replies
  (`src/services/audio_client/src/audio_client.cpp:286-288`, read-only).

Docs: `audio-applet.md` focused-test rows and `testing-harness.md` audio
section updated to describe the four poison controls and the executed
stale-owner check.

## Verification evidence (all commands actually run)

Builds (briefed recipe targets; both profiles):

    cmake --build <ROOT>/debug   --parallel 3 --target qindaqt_audio_applet_controller_tests qindaqt-shell qindaqt-shell-preview
    cmake --build <ROOT>/release --parallel 3 --target qindaqt_audio_applet_controller_tests qindaqt-shell qindaqt-shell-preview
    → both exit 0 (Debug 15/15 steps, Release rebuilt identically; only the
      controller test, shell runtime TUs, and relinks were affected)

Tests — Debug (`<ROOT>/debug`):

    ctest -R '^qindaqt\.audio-applet-' → 6/6 pass
      (includes qindaqt.audio-applet-boundary with pass line
       "3 files and 4 poison rejections")
    ctest -R '^qindaqt\.(shell-runtime-catalog|shell-runtime-options|notification-center-applet-offscreen|applet-host-policy|applet-host-handshake|applet-host-lifecycle)$'
      → 6/6 pass, including #411 notification-center-applet-offscreen
    ctest -R '^qindaqt\.(applet-manifest|applet-catalog|applet-runtime-resolution)$' → 3/3 pass
    ctest -R '^qindaqt\.(power|bluetooth)-applet-(offscreen|controller)$' → 4/4 pass

Tests — Release (`<ROOT>/release`): identical selectors → 6/6, 6/6, 3/3, 4/4 pass.

Boundary gates, direct (worktree root and fresh detached checkout of the
candidate, both exit 0):

    cmake -DSOURCE_ROOT=$PWD -P tests/shell/audio_applet/check_boundary.cmake
      → "passed (3 files and 0 poison rejections)"
    cmake -DSOURCE_ROOT=$PWD -DPOISON_ROOT=<ROOT>/boundary-poison-check -P tests/shell/audio_applet/check_boundary.cmake
      → "passed (3 files and 4 poison rejections)"; fixture dir removed
    cmake -DSOURCE_ROOT=$PWD -DPOISON_ROOT=<ROOT>/rt-poison-check -P tests/shell/audio_applet/check_runtime_boundary.cmake
      → "passed (7 files and poison rejection)"
    fresh detached worktree of caaf7d9 (git worktree add --detach): stub files
      byte-identical to worktree (cmp), pure gate from that checkout passes
      with 4 poison rejections; worktree removed afterwards.
    Manual non-vacuity proof: scratch root + service-internal include appended
      → exit 1 with "non-boundary include 'qindaqt/services/audio_service/...'".

P1-1 clean-tree proof: after committing, `git status --porcelain` is empty
(worktree == candidate tree), `git ls-tree -r HEAD -- tests/shell/qml/imports/`
shows both AudioApplet stub blobs in the commit, and
`ctest -R '^qindaqt\.notification-center-applet-offscreen$'` was rerun
post-commit in both profiles: 1/1 pass each — the row passes from the exact
committed tree.

Static gates (worktree root, all exit 0):

    ./tools/validate-docs → "Validated 117 Markdown documents and mkdocs.yml navigation."
    mkdocs build --strict --site-dir <ROOT>/site → built, no warnings
    ./tools/check-source-shape → exit 0 (warning only: tst_audio_applet_controller.cpp
      at 563 non-blank lines, under the 600 hard gate)
    git diff --check → clean

Not run (per brief): `tests/session` nested-compositor rows, host D-Bus
system/session services, hardware, uinput, network.

## Bounded caveats

- The repair is deliberately limited to the four verdict findings plus their
  doc rows; no product behavior changed (one test gained an executed stale-owner
  assertion, one comment was corrected, one test-only gate gained its committed
  negative control, two test-stub files entered git).
- `tst_audio_applet_controller.cpp` (563 non-blank lines) exceeds the 500-line
  decomposition-review threshold; `./tools/check-source-shape` exits 0 and this
  was pre-existing growth from the original candidate, not new in this repair.
- No JSON files changed in this repair.
