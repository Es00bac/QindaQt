# Portal permissions Settings

The appended `portal-permissions` route lists remembered ScreenCast and RemoteDesktop grants and offers a Revoke button for each application and family. A refresh action reports loading, empty, and unavailable states. Revoking makes a future restore request ask again; it does not close an active portal session or remove transient grants held in the frontend's memory. The [Settings Center](settings-center.md) owns navigation; [ADR-0339](../adr/0339-use-frontend-permission-store-for-settings-revocation.md) records storage ownership.

## Existing-store boundary

The route owns one GUI-thread asynchronous `PortalPermissionsModel`, borrowing an injected session bus connection. Its QML singleton composes that model over the ordinary session bus. The existing frontend-owned `org.freedesktop.impl.portal.PermissionStore` service at `/org/freedesktop/impl/portal/PermissionStore` remains the sole persistent authority. The route never opens the permission database, creates a store, writes Settings1, starts a backend service, or decodes restore data. QML receives copied application/family labels and an opaque row key only. Labels use plain text.

The installed interface is PermissionStore version 2. The exact official xdg-desktop-portal 1.20.4 release archive SHA256 is `9528eb3b060b88ac82f294fbdc6af5f4d3adfa42575f2cd816cab3d3e0a7a68c`. Its `src/screen-cast.c` and `src/remote-desktop.c` choose the `screencast` and `remote-desktop` tables. `src/xdp-session-persistence.c` stores a UUID restore token as resource ID, a single application-to-`["yes"]` permission map, and opaque restore data, and revokes that resource with standard `Delete(table, id)`. Source evidence comes from the [official release](https://github.com/flatpak/xdg-desktop-portal/releases/tag/1.20.4); no upstream implementation is copied into this module.

## Admission and failure truth

Refresh resolves the current unique store owner, calls `List` for only those two tables and `Lookup` for UUID entries, and publishes only singleton `yes` grants. Shared, non-UUID and non-grant resources are ignored; other tables are untouched. Empty app IDs are labeled Unsandboxed application. At most 512 IDs and a ten-second complete refresh are admitted; each D-Bus call has a two-second timeout.

A selected revoke must match the confirmed row and current owner. A fresh Lookup must still identify the same sole application before standard Delete is sent to that unique owner. Delete success triggers a new listing. Refusal, changed grant, unavailable service, malformed reply, timeout, or owner replacement clears rows and disables mutation; no delete is replayed. Changed notifications request refresh, coalescing during an operation. Owner replacement invalidates outstanding replies.

PermissionStore has no compare-and-delete transaction. The recheck protects against already changed shared entries; the frontend's UUID-per-app ownership contract remains necessary between Lookup and Delete. Other resource IDs and tables are never deleted. An active frontend session may save a new grant after a revoke; users must end that session to keep it from being remembered again. The page explicitly describes active-session limits.

## Verification

The focused private-bus gate exercises real D-Bus List/Lookup/Delete serialization, selected deletion with unrelated resources preserved, refusal, grant replacement, and owner loss with a late reply. The public route gate verifies the appended descriptor and actual active page Loader rather than treating registration alone as construction. No focused test reads or writes the user's live PermissionStore. Compilation and gate results are recorded in the exact candidate handoff; source preparation is not a passing runtime claim.
