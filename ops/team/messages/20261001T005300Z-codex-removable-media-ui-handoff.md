# Removable-media UI candidate handoff

- Worker: codex-removable-media-ui
- Candidate: `adb4ff12cadab9815545fe281877aa8b3508c57f`
- Manager base included: `2f5b58a13d2bbec49ab7ef66182584e631a87262`
- Worktree: `/home/cabewse/work_space/container-wm-removable-media-ui`
- Branch: `feature/removable-media-ui`
- Time: 2026-10-01T00:53:00Z
- Requested action: independent exact-commit review, then manager integration and combined-tree/session/package gates.

## Changed paths

Assigned presentation files only: `src/apps/removable_media/{CMakeLists.txt,Main.qml,MediaDetails.qml,FormatDialog.qml,main.cpp,media_appearance.cpp,media_appearance.h,org.qindaqt.RemovableMedia.desktop,tests/tst_media_qml.cpp}` plus this worker's board record/messages. Manager-owned backend, session, and documentation arrive only through merged manager commits.

## Outcome and verification

One activation-only session instance owns the resident watcher. The launcher opens its graphical media window; closing hides it. The window offers mount/open, read-only mount, remembered future insertion choices, passphrase unlock with field clearing, unmount, safe removal, and an attachment-captured separate erase confirmation. Writable mounted format targets require explicit unmount; exact device text and filesystem selection gate erasure. No window operation executes a shell command. Empty/audio discs explain their missing data filesystem. Safe removal remains available on drives without Eject/PowerOff capability.

- Configure exit 0: `cmake -S /tmp/qindaqt-removable-media-ui-harness -B .cache/removable-media-ui-build -G Ninja -DCMAKE_PREFIX_PATH=/home/cabewse/work_space/container-wm-plasma-build/.cache/fork-stage/usr -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug`. This narrow temporary harness includes themes, design tokens, Controls, Settings protocol/client, appearance and removable media only; it does not alter production sources.
- Build exit 0: `cmake --build .cache/removable-media-ui-build --target qindaqt-removable-media qindaqt_media_qml_tests qindaqt_media_policy_tests qindaqt_media_udisks_tests -j 3`, with strict warnings.
- CTests exit 0: `ctest --test-dir .cache/removable-media-ui-build -R '^qindaqt.media-' --output-on-failure`, 3/3. CMake launches the UDisks row under its own `dbus-run-session` and disables the host system bus. Final presentation-only change was rebuilt and the QML row rerun, exit 0, 12/12 QtTest totals (ten behaviors plus setup/cleanup).
- Fixture-only executable probe exit 0 with `--check-ui-contract --theme-directory data/themes`, offscreen/software rendering and nonexistent session/system bus addresses; output `removable-media-ui-ready fixture-only`.
- Private session activation proof exit 0: introspection exports only `Activate`; duplicate watch and normal launcher processes exit 0 while the first watcher remains live. The system storage bus was unavailable throughout.
- QML lint exit 0: `/usr/lib64/qt6/bin/qmllint -I .cache/removable-media-ui-build/qml --unqualified disable` on Main, MediaDetails and FormatDialog. Context-property warnings are disabled; no property/type warnings remain. `git diff --check` exit 0.
- Actual wide-window and captured mounted-format screenshots visually inspected; compact 560x540 and wide 760x720 geometry checks ensure preferences and Format do not overlap. QML warning list is empty; the offscreen platform emits its expected unsupported-raise warning when testing activation.

## Artifacts, caveats, and help

Ignored screenshots:

- `.cache/removable-media-ui-build/removable-media.png`
- `.cache/removable-media-ui-build/removable-media-format.png`

No physical media were mounted/formatted by this worker. Physical insertion/ejection, installed Portage deployment, existing-media mount, independent review, and integrated session gates remain manager work. Product documentation and strict MkDocs gate remain manager owned. The UI calls the saved ignore mode **Do nothing**, and the ask mode **Ask me each time**.

Read the First-party queue and local removable-media threads after handoff. Concrete help offer: reproduce exact review failures using the synthetic storage backend, repair owned UI/main/CMake files in this preserved worktree, and regenerate reviewed wide/compact/dialog captures. Awaiting a routed review finding rather than claiming unrelated paths.
