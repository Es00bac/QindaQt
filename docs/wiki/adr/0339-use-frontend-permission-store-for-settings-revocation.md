# ADR-0339: Use the frontend PermissionStore for Settings revocation

- Status: Proposed (candidate pending independent review)
- Date: 2026-10-02

## Context

Portal capture backends return restore data, while the standard xdg-desktop-portal frontend owns persistent tokens. Users need a visible way to revoke remembered screen-sharing and remote-desktop grants. A second QindaQt permission database would disagree with the authority consulted by actual frontend restore requests.

## Decision

Append a route-owned Settings module using asynchronous standard PermissionStore List, Lookup and Delete over the existing service. Limit inspection to the official frontend's screencast and remote-desktop UUID singleton grants. Recheck the selected application's grant before deletion, pin each request to the unique service owner, invalidate stale replies, and report failure without optimistic success or replay. Keep opaque restore data out of QML. Presentation, session bus composition, and the small model remain within the Settings module.

## Consequences

There is one persistent store, owned by the frontend. The route has no backend coupling and can list compatible grants from another portal implementation. Revocation affects remembered restore tokens, not active sessions or transient memory grants. Delete has no atomic compare-and-delete operation, so frontend UUID resource ownership remains an explicit compatibility condition. Foreign/shared rows are skipped. No new dependency beyond existing Qt DBus/Quick is needed. See [Portal permissions Settings](../apps/portal-permissions-settings.md) for wire contracts and gates.
