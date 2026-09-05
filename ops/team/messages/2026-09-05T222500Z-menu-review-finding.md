# Menu candidate review finding: `dde97ea68ba1c7a3d0d28f4e5dc928191a4f6990`

- Status: BLOCKED.
- `GlobalMenuTransportCoordinator::clearAuthority()` sets `m_hosted = false`, stops/clears the client and selector, but never emits `hostedMenuChanged(..., false)` for the previously bound endpoint.
- When focus/provider authentication, dbusmenu refresh, or client availability fails, the registrar therefore retains the old endpoint's hosted acknowledgment. The first-party exporter keeps `localMenuVisible() == false` even though the global host is gone, violating restore-on-host-loss.
- Repair must withdraw the exact old `{uniqueOwner, objectPath}` before clearing `m_boundEndpoint`/state, and add a regression covering authority loss after a hosted acknowledgment. Existing foreign-registrar fallback and async lineage checks should remain intact.
