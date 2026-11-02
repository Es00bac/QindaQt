# QindaLink notifications lane handoff

- Branch: `link/notifications` (base db0bc5319).
- Outcome: phone notifications appear on the desktop through a presenter, with
  dismissal both ways, action buttons and reply requests (`Link1.Notifications.Reply`,
  or a "Reply…" button that launches `qindaqt-link --reply` when the app is installed).

## Changed paths

- `docs/wiki/reference/qindalink-protocol-v1.md` (Notifications section only),
  `docs/wiki/reference/link1-v1.md`, `docs/wiki/architecture/qindalink.md`.
- `src/services/link_notifications/**` (new library), `tests/link/tst_link_notifications.cpp`,
  `tests/link/CMakeLists.txt`, `src/CMakeLists.txt` (one line).
- `src/services/link_service`: CMake link line, `ServiceConfig` fields, a `NotificationsModule`
  block in `link1_service.cpp` plus `ExportAdaptors`, options `--recording-notifier`,
  `--config-dir`, `--cache-dir` in `main.cpp`, interface in `org.qindaqt.Link1.xml`.
- Phone: `org/qindaqt/link/notify/**`, `android/NotificationListener.java`,
  `android/NotificationAndroidSource.java`, one registration in `LinkService.java`, the listener
  service in `AndroidManifest.xml`, and an additive `notifyAccess` key in `engine/EngineState.java`
  (outside the lane's listed paths; the UI bridge may read it).
- Tests: `phone/engine-tests/NotifyTest.java`, `CrossNotifyTest.java`, `run-cross-notify.sh`, `run.sh`.

## Evidence

- `build/notifications/tests/link/qindaqt_link_notifications_test`: 14 passed, 0 failed.
- `ctest -R qindaqt.link` (10 rows, includes the existing smoke, bounds, presenter): 100% passed.
- `phone/engine-tests/run.sh`: all suites OK including NotifyTest (49 checks).
- `phone/engine-tests/run-cross-impl.sh` still OK; `run-cross-notify.sh <service>`: 20 checks OK
  (snapshot, icon fetch/verify/cache, update replaces, desktop dismiss, action, Reply, removal, mute,
  introspection, no content in the service log) on a private bus with the recording presenter.
- `mkdocs build --strict` and `tools/validate-docs` pass. `phone/tools/build-apk.sh` builds the APK.

## Caveats

- **No live phone check.** `adb install -r` of this worktree's APK fails with
  INSTALL_FAILED_UPDATE_INCOMPATIBLE (the installed `org.qindaqt.link` uses another debug key). I did
  not uninstall it. Recipe for the PM (architecture page, "Notifications"): sign with the installed
  key, `cmd notification allow_listener org.qindaqt.link/org.qindaqt.link.android.NotificationListener`,
  run the service with `--recording-notifier`, post with `cmd notification post`.
- `PendingIntent.send` for actions that open an activity may be blocked by Android's background
  activity launch rules; that returns `internal`. Broadcast and reply actions are unaffected.
- The QindaLink app's reply window (`qindaqt-link --reply`) and inline reply in QindaQt's own
  notification server are follow-ups.
- Production `FdoPhoneNotificationPresenter` is covered by the interface contract and fakes, not by a
  test against a notification server.

## Requested next action

Integrate; sign a build with the installed key and run the live recipe when the phone is available.
