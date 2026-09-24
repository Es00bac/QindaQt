# Update — W6 screen saver preview repair

- Worker: Luna (`luna-w6-screensaver-preview`)
- Branch: `worker/claude-w6-screensaver-preview-20260923`
- Confirmed the chosen `org.freedesktop.ScreenSaver.Inhibit` interface is honored by KScreenLocker before its automatic idle-lock timeout. The request is acquired before preview presentation, explicitly uninhibited at completion, and bound to a dedicated connection so process exit also releases it.
- Added blank activation timeout and 60-second maximum preview lifetime; all displays must show or opened windows are closed. Saver crash/nonzero exit code reaches a dedicated accessible Preview error label.
- Focused build passed. `ctest --test-dir build/dev -R screensaver --output-on-failure` passed 6/6. The full build and full Settings test suite remain to run.
