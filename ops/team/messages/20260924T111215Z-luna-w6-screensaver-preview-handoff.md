# Candidate handoff — W6 screen saver preview repair

- Worker: Luna (`luna-w6-screensaver-preview`)
- Branch: `worker/claude-w6-screensaver-preview-20260923`
- Exact candidate commit: `8b9cb78828ab83504eeff703080678c3e31fc5ce`
- Base reviewed: `b443f394483ee758f15fc6f0b2963aa3de2e563a`
- Candidate commit is pushed to the `hub` branch.

## Contract landed

- Acquire KScreenLocker's public `org.freedesktop.ScreenSaver.Inhibit` cookie before starting a preview; fail closed when that authority cannot grant it. Explicitly uninhibit on normal finish and drop the dedicated session-bus connection on every teardown path, including Settings exit.
- Bound every preview to 60 seconds maximum. Blank preview requires a shown window on every available screen, rolls back every opened window after any show failure, and reports/ closes on activation failure. Saver crash and nonzero exit status/code reach a separate accessible Preview error message.
- `qindaqt-desktop-controls` continues to use KIdleTime to launch the unlocked idle saver; KScreenLocker remains the automatic lock authority and honors the inhibitor. ADR-0259 and the app/architecture wiki describe this boundary.

## Changed paths

- `docs/wiki/adr/0259-preview-screen-savers-without-the-lock-screen.md`
- `docs/wiki/apps/screensaver-settings.md`
- `docs/wiki/architecture/desktop-controls.md`
- `ops/team/workers/luna-w6-screensaver-preview.md`
- `ops/team/messages/20260924T094725Z-luna-w6-screensaver-preview-claim.md`
- `ops/team/messages/20260924T100857Z-luna-w6-screensaver-preview-update.md`
- `ops/team/messages/20260924T102900Z-luna-w6-screensaver-preview-update.md`
- `ops/team/messages/20260924T105300Z-luna-w6-screensaver-preview-update.md`
- `ops/team/messages/20260924T110307Z-luna-w6-screensaver-preview-update.md`
- `ops/team/messages/20260924T110600Z-luna-w6-screensaver-preview-update.md`
- `src/apps/settings/screensaver/CMakeLists.txt`
- `src/apps/settings/screensaver/include/qindaqt/apps/settings_screensaver/screensaver_preview.h`
- `src/apps/settings/screensaver/include/qindaqt/apps/settings_screensaver/screensaver_settings_model.h`
- `src/apps/settings/screensaver/qml/ScreensaverPage.qml`
- `src/apps/settings/screensaver/screensaver_idle_lock_inhibitor.cpp`
- `src/apps/settings/screensaver/screensaver_idle_lock_inhibitor_p.h`
- `src/apps/settings/screensaver/screensaver_preview.cpp`
- `src/apps/settings/screensaver/screensaver_settings_model.cpp`
- `tests/apps/settings/screensaver/CMakeLists.txt`
- `tests/apps/settings/screensaver/screensaver_model_test_support.h`
- `tests/apps/settings/screensaver/screensaver_preview_test_support.h`
- `tests/apps/settings/screensaver/tst_screensaver_page.cpp`
- `tests/apps/settings/screensaver/tst_screensaver_preview.cpp`
- `tests/apps/settings/screensaver/tst_screensaver_preview_failures.cpp`
- `tests/apps/settings/screensaver/tst_screensaver_preview_failures.h`
- `tests/apps/settings/screensaver/tst_screensaver_settings_model.cpp`

## Verification

- Built all 96 Settings-related executable targets with `-- -j8 -l20`: exit 0.
- Built `qindaqt_compositor` for the compositor integration case with `-- -j8 -l20`: exit 0.
- `ctest --test-dir build/dev -R screensaver --output-on-failure`: exit 0, 6/6.
- `ctest --test-dir build/dev -L settings --output-on-failure`: exit 0, 133/133.
- `./tools/validate-docs`: exit 0, 389 Markdown documents and navigation validated.
- `mkdocs build --strict --site-dir build/dev/docs-site`: exit 0.
- `git diff --check`: exit 0.

## Bounded caveat and next action

The inhibitor acquisition/release contract is covered by a fake ScreenSaver D-Bus service, and KScreenLocker's handling was checked in its implementation. This offscreen run did not wait for a real desktop idle-lock countdown to expire. Please independently review exact commit `8b9cb78828ab83504eeff703080678c3e31fc5ce`; integrate it if accepted and rerun the affected gates on the integration branch.
