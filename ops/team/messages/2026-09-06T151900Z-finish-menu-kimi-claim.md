# Claim: Global Menu flash/inconsistency repair (finish-menu-kimi)

- **From:** finish-menu-kimi (Moonshot Kimi CLI)
- **Time:** 2026-09-06T15:19:00Z
- **Lane:** `fix/finish-menu` at exact base `8958c38f0a9cca5c7906ad62e2fa54afb486a05f`, worktree `/home/cabewse/work_SPaC3/container-wm/.cache/finish-menu`
- **Ownership:** `src/shell/global_menu/**`, `src/app_shell/menu_export/**`, focused tests (`tests/shell/global_menu/**`, `tests/app_shell/tst_application_menu_export*.cpp`), `docs/wiki/shell/global-menu.md`. No shared-path edits anticipated; the compositor/client invalidation cadence stays as-is (see below).

## Diagnosis (code-verified)

The user's "global menu flashes and is inconsistent" is one mechanism with five compounding links, all reachable inside my owned paths:

1. `src/compositor/src/shellwindowidentity.cpp:204-214` — the identity snapshot's canonical state embeds `actionRevision` (the visibility revision). `src/compositor/kwin/kwinshellvisibilitypublisher.cpp:181-191` republishes on any visible window's geometry/maximize/minimize/desktop change, so `ActiveWindowIdentityChanged` fires on events that are not focus moves (e.g. dragging any window).
2. `src/shell_window_actions_client/src/shell_window_actions_client.cpp:209-221` — the client withdraws its snapshot on that signal and emits `identityChanged` again on the reread reply, even for identical content.
3. `src/shell/global_menu/composition/src/global_menu_transport_coordinator.cpp:84-103,302-315` — `refreshFocus()` on the withdrawn snapshot hits `clearAuthority()`: client stopped, selector cleared, `publishUnavailable()`. The reread then triggers a full `bindRegistration()` with a new client and GetLayout round trip.
4. `bindRegistration()` (`global_menu_transport_coordinator.cpp:199`) calls `publishUnavailable()` before the async tree exists.
5. `src/shell/global_menu/applet/qml/GlobalMenuApplet.qml` collapses to zero extent when unavailable (panel reflow), and `globalmenuappletaccess.cpp:151` embeds a newly incremented generation into every projected item, so content-identical republishes still fire `itemsChanged` and rebuild every delegate.

This matches the independent finish-ux-glm finding 3.

## Fix plan (owned paths only)

- Coordinator: same-window/same-endpoint identity changes renew in place (re-authenticate, re-adopt, re-stamp) with no client teardown and no `publishUnavailable`; new-provider binds keep the previous presentation (non-actionable) until the new tree publishes; authority revocation on withdrawal stays synchronous and fail-closed, while the presentation clears only after a bounded grace so transient invalidation/reread cycles never collapse the panel.
- Facade/QML: retained entries render dimmed and non-actionable during transition; zero extent only when genuinely unavailable after the grace; skip applet republish when exported content is unchanged.
- No old app menu is ever actionable for a new focus: invocation authority (selector/client/exporter) is dropped synchronously on every withdrawal; only pixels linger briefly, dimmed.
- Regression tests: same-window generation churn keeps menu+delegates stable; transient withdrawal within grace never collapses; genuine focus loss clears after grace; unchanged-content reread emits no `itemsChanged`.

No compositor or window-actions-client changes requested at this time: the coordinator can absorb the invalidation cadence. If verification shows the cadence itself must change, I will post a narrow request for `src/shell/runtime/globalmenuappletcomposition.cpp` or the compositor identity publisher.

Worker record: `ops/team/workers/finish-menu-kimi.md`.
