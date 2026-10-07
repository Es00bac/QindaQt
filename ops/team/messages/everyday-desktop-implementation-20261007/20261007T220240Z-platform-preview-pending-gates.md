# Future capture exact pending compiler packet

Source authored ec10d63dde45d56621a14bfad14bcfc63a946c21; board-only65278f94a. Worktree /home/cabewse/work_SPaC3/container-wm.worktrees/everyday-preview-capture-20261007. No compiler commands run. Manager queues this immediately after Astra focusedlease release.

Configure own .cache/ed-preview-capture-build:
cmake -S . -B .cache/ed-preview-capture-build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/usr -DKDE_INSTALL_LIBEXECDIR=libexec -DBUILD_TESTING=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=OFF -DQINDAQT_BUILD_KWIN_PLUGIN=OFF -DQINDAQT_NATIVE_POWER_EXCLUSIVE=OFF -DQINDAQT_KWIN_BUILD_ACTIVITIES=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON

Read actual installed Portage MAKEOPTS first; require configured -j24 -l24 unchanged. Build:
cmake --build .cache/ed-preview-capture-build --target qindaqt-shell-preview qindaqt_shell_capture_tests qindaqt_screenshot_capture_error_tests -- -j24 -l24

Tests:
ctest --test-dir .cache/ed-preview-capture-build -R '^qindaqt\.shell-capture-(matrix|errors)$' --output-on-failure --no-tests=error

Private HOME/XDG, QT_QPA_PLATFORM offscreen, software/basic Qt, blocked session/systembus addresses. Matrix retains40 normalprofile/resolution rows, adds3DPR rows, outputerror and3no-window oversizedinputs; focusederror target adds purefinite/nativebounds and logical/deferredmismatch. Counts remain pending observed execution.

Final docs: python3 tools/validate-docs; mkdocs build --strict in ignored cache; git diff --check. Freeze passing exactcandidate and root independentreview afterward. No install/service/hostsession actions. r16/source2188/artifact bytes remain immutable.
