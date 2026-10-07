# Missing PAM service reproduction verified

- Timestamp: 2026-10-07T14:46:55.082460+00:00
- Base: 46e6a74dc0de6b279ca8c2a24e50d3f86634d925
- Worktree: /home/cabewse/work_SPaC3/container-wm-unlock-20261007
- Branch: worker/unlock-20261007
- Changed product paths: src/lock_authentication/tests/tst_worker_channel.cpp; docs/wiki/architecture/native-session-lock.md

The new missingServiceHasNoPrompt test uses pam_start_confdir in the non-installed owned worker, a temporary directory with no qindaqt-lock service, and a deny-only other stack. The first and only frame is Result/Denied with the current token; no Secret/Visible conversation precedes it. The existing positive row receives a real Secret prompt, supplies only fixture-response, and obtains approval only after the private module's authentication and account phases. No host PAM configuration or credential is read/executed by these tests.

## Executable verification

- CMake configuration: BUILD_TESTING=ON; shell/prod-shell/KWin plugin/viewer/system-monitor/removable-media/OBS/QindaLutris/audio-live-runtime OFF; Debug; prefix/usr; KDE_INSTALL_LIBEXECDIR=libexec, LIBDIR=lib64, USE_QT_SYS_PATHS=ON. Initial generic KDE libexec default mismatch failed configure; selecting actual installed fork paths repaired the test configuration. Final configure/generate exit0.
- cmake --build .cache/unlock-build --target tst_lock_authentication tst_native_pam_conversation tst_lock_worker_channel qindaqt-private-lock-greeter -j2: exit0,267 actions. Isolated source/build root; no install.
- env QT_FATAL_WARNINGS=1 ctest --test-dir .cache/unlock-build --output-on-failure --no-tests=error -V -R '^lock\.(authentication|native-pam-conversation|worker-channel)$': exit0,3/3 CTests,37 Qt checks (12,10,15), zero failure/skip.
- mkdocs build --strict --site-dir .cache/unlock-wiki: exit0.
- python3 tools/docs_validation.py: exit0,510 Markdown documents/navigation.
- git diff --check: exit0.

Manager owns configuration-package delivery and ADR0349. This candidate does not alter greeter/protocol/unlock authority, host PAM or system packages. Real session pixels/focus/owner password/physical unlock remain owner-controlled acceptance. October4 gate masked PAM and proved only production launch/role/frame. No private compositor was started; runtime/compiler resources released. Exact candidate will be independently reviewed before integration.
