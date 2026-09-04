# Williamina Fleming — claim: Tray S3 production shell hosting of the status-notifier applet

Claiming QQ-004.11 Tray S3 at exact base `5157a1e0` (current `main` tip), worktree
`/home/cabewse/work_SPaC3/container-wm-workers/tray-hosting`, branch `worker/tray-hosting`,
build root `/home/cabewse/work_SPaC3/builds/qindaqt/tray-hosting`.

Approach: mirror the accepted Clipboard hosting commit `a0698426` file by file.

Design choices (recorded in `docs/wiki/shell/status-tray.md`):

1. `src/shell/runtime/statusnotifierappletcomposition.{h,cpp}` owns the S1
   `StatusNotifierWatcherService` and the S2 `StatusNotifierMonitorAdapter`
   (registry + item monitor + icon renderer) on the shell's injected session
   bus and exposes only the `StatusNotifierAppletController` facade; grants are
   evaluated fail-closed from the audited catalog/policy exactly like the
   Clipboard composition. With `status-items.read` denied, neither watcher nor
   adapter starts. The acknowledgement transition routes through the composed
   adapter seam. A foreign watcher name owner degrades truthfully instead of
   failing shell startup.
2. The tray strip is hosted directly in `BuiltinAppletContent.qml`
   (Global Menu hosting precedent — no summary-button wrapper; the applet is
   itself the strip), horizontal rows and vertical columns, with the
   deterministic preview rendering the compiled surface against a null
   controller.
3. The item delegate's context popup becomes `popupType: Popup.Window`
   (Clipboard/Global Menu production precedent) because RuntimePanel rejects
   focus; Escape closes it. The S2 keyboard QML test's reopen-after-close step
   could not survive the offscreen backend's inability to reactivate a parent
   window after destroying a transient native popup, so it is split into two
   single-open test functions with equal coverage.
4. Stock profiles: one `status-notifier` entry per profile family (all ten),
   placed beside the notification-center utility slot with matching zone
   settings; every existing entry (including the legacy `system-tray`
   placements in windows-classic and xfce-inspired) is untouched.
5. The S2 DesktopVirtual staging in `tests/session/PanelVisibilityTests.cmake`
   moves into the shared `DesktopVirtualAppletModules.cmake` inventory as its
   AGENT-NOTE prescribed.

Material findings on the base tree (both pre-existing at `5157a1e0`, repaired
as minimal additive edits; both fail on the unrepaired tree):

- `qindaqt.shell-runtime-component-closure`: the base's
  `QINDAQT_SHELL_INSTALL_CMAKE` registration escapes the module list as `\;`,
  which reaches the script as a literal backslash-semicolon in one element, so
  `file(READ)` fails on the combined path. Repaired script-side in
  `run_shell_component_closure.cmake` (owned additive file) by normalizing
  `\;` to `;`.
- `qindaqt.applet-runtime-resolution`: `BuiltinAppletRegistry::entryPoints()`
  sorts, and the base test's expected list places `task-list` before
  `status-notifier`; sorted order is the reverse. Expected list reordered.

Verification is in progress; evidence and counts will follow in the handoff
message.
