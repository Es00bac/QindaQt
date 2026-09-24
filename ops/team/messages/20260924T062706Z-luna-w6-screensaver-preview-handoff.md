# Handoff — W6 screen saver preview

- Worker: Luna (`luna-w6-screensaver-preview`)
- Branch: `worker/claude-w6-screensaver-preview-20260923`
- Base: `2e415cad0d1c2ece5b3e3af6962e1d51a9b6f347`
- Candidate commit: `a5300bbbd6e6a49708f897a8144e8075828b71a4`
- Candidate commit is pushed to `hub`.
- ADR: [ADR-0259](../../../docs/wiki/adr/0259-preview-screen-savers-without-the-lock-screen.md); clause 4 of ADR-0226 is marked superseded, preserving its original decision text.

## Contract delivered

Every discovered saver preview resolves its own catalog program and starts it with the public catalog argument list. `blank` opens a Settings-owned full-screen black window on each screen and closes on any key, Escape, click, or pointer movement. Settings preview has no greeter path. `none` and unknown tokens remain unavailable; overlapping previews are refused; write-in-flight button behavior and page error reporting remain covered. Preview does not request or inhibit idle locking.

## Changed paths

- `src/apps/settings/screensaver/CMakeLists.txt`
- `src/apps/settings/screensaver/include/qindaqt/apps/settings_screensaver/screensaver_preview.h`
- `src/apps/settings/screensaver/screensaver_preview.cpp`
- `src/apps/settings/screensaver/screensaver_route_composition.cpp`
- `src/apps/settings/screensaver/screensaver_route_composition.h`
- `src/apps/settings/screensaver/screensaver_settings_model.cpp`
- `tests/apps/settings/screensaver/CMakeLists.txt`
- `tests/apps/settings/screensaver/screensaver_model_test_support.h`
- `tests/apps/settings/screensaver/tst_screensaver_page.cpp`
- `tests/apps/settings/screensaver/tst_screensaver_preview.cpp`
- `tests/apps/settings/screensaver/tst_screensaver_settings_model.cpp`
- `docs/wiki/adr/0226-configure-the-screen-saver.md`
- `docs/wiki/adr/0259-preview-screen-savers-without-the-lock-screen.md`
- `docs/wiki/adr/index.md`
- `docs/wiki/apps/screensaver-settings.md`
- `mkdocs.yml`
- `ops/team/workers/luna-w6-screensaver-preview.md`
- `ops/team/messages/20260924T053501Z-luna-w6-screensaver-preview-claim.md`
- `ops/team/messages/20260924T060352Z-luna-w6-screensaver-preview-update.md`
- `ops/team/messages/20260924T062706Z-luna-w6-screensaver-preview-handoff.md`

## Verification

- `cmake --preset dev` — exit 0.
- Focused build of `qindaqt_settings_screensaver`, its QML plugin, and the four Settings saver test targets with `-- -j8 -l20` — first attempt exit 1 because the new preview test did not yet link `Qt6::Quick`; after adding that dependency, rerun exit 0.
- Build `qindaqt_settings1_screensaver_preferences_tests` and `qindaqt_screensaver_catalog_tests` with `-- -j8 -l20` — exit 0.
- Rebuild `qindaqt_settings_screensaver` and `qindaqt_screensaver_preview_tests` after the blank-window input filter change — exit 0.
- Rebuild `qindaqt_screensaver_preview_tests` after adjusting the pointer-motion test destination — exit 0.
- `ctest --test-dir build/dev -R '^qindaqt\.settings-screensaver-preview$' --output-on-failure` — initial run exit 8 from the pointer-motion test targeting the existing cursor position; rerun after fixing the test destination passed 1/1, exit 0.
- `ctest --test-dir build/dev -R screensaver --output-on-failure` — initial run exit 8, 5/6 passed from that same pointer-motion test; rerun passed 6/6, exit 0.
- `ctest --test-dir build/dev -L settings --output-on-failure` — exit 8: 30 passed, 14 failed, 89 not run, 0 CTest timeouts out of 133. The worktree built focused targets only, leaving other executables unavailable. The compositor case was rerun individually and failed again because `build/dev/src/services/settings_service/qindaqt-settings-service` is absent. No suite timeout needed an individual rerun.
- `./tools/validate-docs` — exit 0; validated 389 Markdown documents and `mkdocs.yml` navigation.
- `git diff --cached --check` before candidate commit — exit 0.

`mkdocs build --strict` and a live desktop/idle-lock trial were not run in this worktree. The worker brief says strict MkDocs is unavailable on qinda and the manager runs it on the laptop; the desktop behavior remains for review/live validation.

## Requested next action

Have a different worker review exact candidate commit `a5300bbbd6e6a49708f897a8144e8075828b71a4`, then route the accepted candidate for manager-branch integration.
