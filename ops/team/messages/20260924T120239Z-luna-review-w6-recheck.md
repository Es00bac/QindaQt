# W6 screensaver preview recheck

- Reviewer: `luna-review-w6`
- Verdict: **ACCEPT**
- Candidate: `08a2b99021a5a3b4bda9168645b663bcc164a302`
- Repair commit: `8b9cb78828ab83504eeff703080678c3e31fc5ce`
- Base: `2e415cad`
- Reviewed: full candidate diff against base and repair diff `b443f394..08a2b990`; clean `git diff --check 2e415cad..08a2b99021a5a3b4bda9168645b663bcc164a302`.

## Recheck of prior findings

1. **Blank preview cannot remain indefinitely.** Preview duration is clamped to 60,000 ms. The blank preview is dismissed at the deadline, and a saver process is terminated then killed after its grace period. Blank preview activation has a bounded focus check; inability to focus dismisses the preview and reports an error.
2. **Display failure rolls back.** The blank preview verifies every display window became visible. If any fails, all opened windows close and startup reports an error. Display topology changes also dismiss the preview.
3. **Saver exit failures are surfaced.** Unexpected nonzero exit and crash produce an error containing the exit code/status, which the settings model exposes in the page's accessible alert. Tests cover nonzero exit and crash.
4. **Automatic idle locking is inhibited before preview.** The dedicated `org.freedesktop.ScreenSaver.Inhibit` call is bounded and fail-closed; no preview starts without a valid cookie. Normal completion explicitly calls `UnInhibit`, and all cleanup paths drop the dedicated bus connection.

## Lock-authority verification on qinda

The live Qinda session's `org.freedesktop.ScreenSaver` owner was `kwin_wayland` (PID 23479); its introspected `/ScreenSaver` object exposes `Inhibit(ss) -> u` and `UnInhibit(u)`. The installed packages are KWin and KScreenLocker 6.6.6. I checked the exact [KScreenLocker 6.6.6 source archive](https://download.kde.org/stable/plasma/6.6.6/kscreenlocker-6.6.6.tar.xz) against the local Gentoo Manifest:

- `interface.cpp` registers the freedesktop ScreenSaver service. `Inhibit` records the caller's unique D-Bus service, creates the inhibitor and calls `KSldApp::inhibit()`; `UnInhibit` removes it and calls `uninhibit()`.
- The service-unregistered watcher removes the cookie when the caller's unique D-Bus name disappears. This covers Settings termination/crash when its D-Bus connection closes.
- `ksldapp.cpp` returns without locking when inhibited; its idle-timeout update removes the timer while the inhibit count is nonzero and restores it after the final release.
- KWin's installed-version integration initializes `KSldApp`, so this is the active lock path for the live owner.

The fake-bus tests cover acquisition, release, repeated previews without a retained cookie, fail-closed acquisition, and Settings-side teardown. A timed end-to-end idle-expiry trial and a forced whole-Settings process crash were not run; the crash-release conclusion is supported by KScreenLocker's exact-version D-Bus service-watcher behavior and the teardown tests.

## Verification

- `cmake --preset dev` — configured the detached review worktree.
- Built all 93 CTest Settings-labeled test targets plus `qindaqt-settings`, `qindaqt-settings-service`, `qindaqt-wm`, and `qindaqt_compositor`: 2,466 Ninja steps, exit 0, using `-- -j8 -l20` as required.
- The first `ctest -R screensaver` attempt found four Settings tests passed but two session-only screensaver executables had not been built. I built `qindaqt_settings1_screensaver_preferences_tests` and `qindaqt_screensaver_catalog_tests` using the same limits, then reran the requested filter: **6/6 passed**.
- `ctest --test-dir build/dev -L settings --output-on-failure` — **133/133 passed**, exit 0 (237.16 seconds).
- `./tools/validate-docs` — passed; validated 389 Markdown documents and MkDocs navigation.
- `mkdocs build --strict --site-dir build/dev/docs-site` — passed, exit 0. I independently reproduced it with the already-present `/tmp/qinda-mono-docs-venv/bin/mkdocs`. The worker handoff records this strict MkDocs command. The venv's MkDocs executable and package metadata are timestamped Sep 23, before this W6 work; I found no evidence of a W6 package installation, and neither I nor the review build installed packages.
- The Settings page tests ran in the configured warning-fatal/accessibility mode and passed without QML warnings.

## Review result

The four prior blocking findings are repaired, and the full Settings build and test gates pass. No blocking findings remain. Accept candidate `08a2b99021a5a3b4bda9168645b663bcc164a302` for integration.
