# Tray S2 claim — compiled QST status-notifier applet

- Worker: Sijue Wu (Moonshot Kimi `kimi-code/k3`), slug `sijue-wu`
- Outcome: Tray S2 — compiled QST status-notifier applet over the accepted S1
  transports (controller, compiled `QindaQt.Shell.StatusNotifier` module,
  registration, focused tests, docs). Hosting in the panel is a later lane.
- Exact base: `893805724933b307e165fdefc485df1ae4a13015` (current `main` tip at
  claim; contains tray S1 `e3eacdd`).
- Branch/worktree: `worker/tray-applet` at
  `/home/cabewse/work_SPaC3/container-wm-workers/tray-applet`.
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/tray-applet`, configured
  with the system-KWin initial cache per the lane override.

Design decisions recorded up front (details land in the status-tray wiki page
with the change):

1. New manifest `data/applets/status-notifier.json` (id `status-notifier`,
   entry point `qindaqt.applets.status-notifier`, capabilities
   `status-items.read` / `status-items.activate`) per the lane's owned paths.
   The older `system-tray` manifest stays an accepted catalog contract and keeps
   resolving `implementation-unavailable`; reconciling the two belongs to the
   hosting lane.
2. The applet adapter observes registry changes through a forwarding
   `StatusNotifierEventSink` shim between the monitor and the registry (the S1
   monitor emits only `watcherLiveChanged`). Intent dispatch stays with the
   monitor's `request*` methods, which evaluate and revalidate intents
   internally; the controller fences generation before dispatch and guarantees
   one wire call per admitted gesture.
3. Icons are rendered GUI-thread through the injected S1 icon-renderer seam and
   cross into QML as bounded PNG data URLs (no engine-global image provider);
   placeholder truth comes from the S1 deterministic fallback.
4. The lane brief asks for a DesktopVirtual staging block in
   `tests/session/PanelVisibilityTests.cmake` mirroring the Global Menu block.
   That block moved to `tests/session/DesktopVirtualAppletModules.cmake` in
   `99b06199`, and that inventory is contractually limited to modules imported
   by `BuiltinAppletContent.qml` (not mine — hosting is a later lane and that
   file is outside my shared edits). I will stage
   `QindaQt/Shell/StatusNotifier` into the DesktopVirtual component from
   `PanelVisibilityTests.cmake` exactly where the historical Global Menu block
   lived (inside the session-probe guard), which keeps
   `desktop.virtual.stage-closure`/`package-contract` semantics intact.
5. Descriptor menu payloads (S1-validated, flat, depth <= 4) are projected as
   bounded read-only preview rows; entry activation authority (dbusmenu event
   dispatch) remains the later composition lane. The actionable context-menu
   route dispatches `RequestKind::ContextMenu` through the monitor.

Requested next action: none until handoff; will post the candidate here.
