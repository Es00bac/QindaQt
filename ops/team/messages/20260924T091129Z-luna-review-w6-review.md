# Independent review: W6 screensaver preview

- **Verdict:** REJECT
- **Candidate:** `b443f394483ee758f15fc6f0b2963aa3de2e563a`
- **Base:** `2e415cad`
- **Review worktree:** detached at the exact candidate commit

## Blocking findings

1. **Blank preview has no fallback dismissal if input cannot reach it.** In `src/apps/settings/screensaver/screensaver_preview.cpp:111-136`, the preview shows full-screen windows, requests activation only for the first window, and has no timeout or other fallback. Key dismissal depends on a key event reaching a preview window; pointer dismissal also depends on an event arriving. If the compositor denies activation or there is no usable input device, the black windows can remain indefinitely with Settings covered. The tests send keys directly to the QWindow, so they do not exercise denied activation or real focus delivery. Add a bounded recovery path or another guaranteed way to close the preview, report activation/show failure, and cover the failure path in a test.

2. **A partial multi-display show is accepted as success.** In `src/apps/settings/screensaver/screensaver_preview.cpp:111-123`, `show()` returns true as soon as any window is visible. If another display's window fails to show, Preview remains marked running and reports no error while that display is uncovered. Require every available screen to show successfully; close any windows already opened and report failure if one cannot, with a partial-failure test.

3. **Saver crashes and nonzero exits are not reported.** In `src/apps/settings/screensaver/screensaver_preview.cpp:168-169`, the `QProcess::finished` exit code and status are discarded and the preview emits only `finished()`. `ScreensaverSettingsModel` only republishes status at `src/apps/settings/screensaver/screensaver_settings_model.cpp:69-70`. Once a saver starts, `start()` returns success at `screensaver_preview.cpp:248-256`; if it then crashes or exits nonzero, Settings receives no failure message. Preserve the exit result and publish an error state; cover it with a program that starts and exits unsuccessfully.

4. **The configured automatic lock can still put the password screen over Preview.** ADR-0259 explicitly leaves the normal idle-lock timeout active during Preview (`docs/wiki/adr/0259-preview-screen-savers-without-the-lock-screen.md:33-35`). If Preview remains open past that timeout, the normal lock authority can lock the session and show the greeter/password screen. The Preview implementation itself does not call a greeter or lock service (`screensaver_preview.cpp:245-248`), but the user requirement says Preview must never show the lock screen or ask for a password. Inhibit the normal idle lock for the lifetime of Preview, or otherwise guarantee Preview ends before it can fire, and update the ADR and UI text with that behavior.

## Non-blocking notes

- Preview resolves each discovered saver through the public `ScreensaverCatalog::entry(token)` and passes that entry's exact `arguments`. The idle launcher uses the same catalog entry and argument list at `src/session/desktop_controls/production/screensaver_launcher.cpp:173-179`; there is no duplicated per-saver flag table.
- The Preview path does not start `kscreenlocker_greet` or request a lock directly. Blank uses Settings-owned full-screen black windows and includes key, Escape, click, and pointer-motion dismissal handlers.
- Single-instance admission and disabling Preview during a pending settings write are present. The focused QML test passed with `QT_FATAL_WARNINGS=1`, with no QML warning failure.
- ADR-0259 supersedes only clause 4 of ADR-0226; the earlier ADR remains as history. Documentation validation passed.
- No live compositor/display session was available (`XDG_SESSION_TYPE=tty`, with no X11 or Wayland display), so physical focus delivery, multiple monitor presentation, and live lock-timeout behavior could not be verified.

## Verification

- `cmake --build build/dev --target all -- -j8 -l20` — passed, exit 0.
- `ctest --test-dir build/dev -L settings --output-on-failure` — passed, **133/133**, 0 failed, 248.76 seconds. All settings test targets were built first. No timeout required an individual rerun; no base comparison was needed because the suite had no failures.
- `ctest --test-dir build/dev -R screensaver -V --output-on-failure` — passed, **6/6**.
- `git diff --check 2e415cad..b443f394483ee758f15fc6f0b2963aa3de2e563a` — passed.
- `tools/validate-docs` — passed; validated 389 Markdown documents and `mkdocs.yml` navigation.
- `python3 -m mkdocs build --strict` — unavailable; Python reported `No module named mkdocs`. No dependency was installed, per the reviewer brief.
