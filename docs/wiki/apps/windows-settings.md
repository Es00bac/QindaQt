# QindaQt Settings — Windows & workspaces route

`qindaqt-settings --page windows` edits the four Settings1 schema v2
`windowManagement.*` keys that have live consumers through the session bridge
([ADR-0209](../adr/0209-bridge-window-management-settings-into-kwinrc.md)):

| Key | Type | Default | Consumers |
| --- | --- | --- | --- |
| `windowManagement.focusPolicy` | `click` / `focus-follows-mouse` / `focus-under-mouse` | `click` | kwinrc `[Windows] FocusPolicy`, read by KWin on reconfigure |
| `windowManagement.dockingModifier` | `super` / `alt` / `control` / `disabled` | `super` | kwinrc `[QindaQt] DockingModifier`, rebinding the compositor's exact docking chord (Shift stays in every chord; `disabled` matches no pointer press) |
| `windowManagement.snapDistance` | integer 0–64 | `12` | kwinrc `[Windows] BorderSnapZone` and `WindowSnapZone` |
| `windowManagement.closeContainerPolicy` | `ask` / `close-all` / `ungroup` | `ask` | kwinrc `[QindaQt] CloseContainerPolicy`: a standing decision skips the group close prompt |

`windowManagement.sessionRestore` is defined by the schema and carried into
kwinrc by the bridge, but **reserved**: nothing reads it yet, so the route
neither scopes, reads, nor writes it. The boundary scan rejects any route
source that names the key. The route decision is
[ADR-0210](../adr/0210-windows-and-workspaces-settings-route.md); the navigation
shell that hosts it is the [Settings Center](settings-center.md).

## Truth and mutation

The route's QML module owns one route-local composition root
(`WindowsRouteComposition`, like Clipboard): one independent
`QtSettingsTransport` and a `SettingsClient` scoped to exactly the four keys
above, handed to the page only as the route model (`WindowsSettingsModel`).
The model keeps the last confirmed values and one draft. A snapshot must carry
every scoped key with a schema token or an in-range whole number (integral
floating wire numbers are accepted for the distance); anything else makes the
route Unavailable and revokes edit and apply admission while preserving the
draft.

Edits accumulate in the draft; nothing is written until **Apply**. Because the
public client commits one key per transaction, Apply writes the changed keys
one at a time in schema order (`focusPolicy`, `dockingModifier`,
`snapDistance`, `closeContainerPolicy`), issuing each next key only from the
fresh post-commit snapshot so every write carries the current base revision.
The route never claims one atomic transaction. **Revert** returns the draft to
the confirmed values and is refused while a commit is in flight.

- A `Conflict` reply whose current value already equals the intended value
  counts as applied; any other conflict stops the sequence, keeps the draft,
  reloads current values, and requires an explicit Apply to restate the
  remaining choice.
- A timeout, owner replacement, or transport loss during a write is never
  retried automatically; the outcome is reported as uncertain and the draft is
  kept for an explicit re-Apply.
- Draft setters refuse tokens outside the schema and distances outside 0–64
  (the page reports the distance error inline), so the route never submits a
  value Settings1 would reject.

Once Settings1 confirms a key, `qindaqt-session` writes it into `kwinrc` and
asks KWin to reconfigure; the change is live in the running session without a
restart. The page shows no "restart required" state because there is none.

## Page

Three closed choices (`ComboBox`) and one slider under **Focus**, **Arranging
windows**, and **Window groups**, each with a one-sentence tooltip and
accessible description; the choice labels are presentation only and never
expose the schema tokens. The first focus target is the focus-policy selector,
or Retry when the route is unavailable. Apply is admitted only for a dirty
draft in an editable state and shows busy while Saving; Revert is admitted for
a dirty draft; Retry appears with the degraded notice. The compact layout
(under 560 px) stacks every label above its control.

## Verification

- `qindaqt.settings-windows-model`: exact four-key scope, token and range
  gating, per-key commit order from fresh snapshots, fail-closed malformed
  refresh, conflict stop and explicit re-Apply, uncertain no-replay,
  replacement abort, and choice lists covering every schema token.
- `qindaqt.settings-windows-page`: offscreen page over a stub model — wide and
  compact layouts, admitted focus, accessible roles and descriptions, labels
  not tokens, interactive-only draft writes (activation and slider move), Apply
  and Revert wiring, saving fences, unavailable notice with Retry.
- `qindaqt.settings-windows-boundary` and `-boundary-poison`: public-client
  allow-list (no bridge, compositor, KConfig, or non-public header; D-Bus only
  in the composition root; the reserved session-restore key rejected).
- `qindaqt.settings-windows-installed-route`: relocated stage runs `--page
  windows` resident with host buses poisoned and fails closed when its module
  is withheld.
- Settings Center suites pin the route at index 12 with stable order and
  traversal; `check_route_construction` and `check_installed_routes` include
  it.
