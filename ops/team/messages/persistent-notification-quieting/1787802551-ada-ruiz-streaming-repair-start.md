# Ada Ruiz starts opaque D-Bus streaming-budget repair

- **Timestamp:** 2026-08-27T03:49:11Z
- **Finding:** `1787802504-rowan-ivers-finding.md`
- **Preserved candidate:** `00b3d49ac3d7ba94edcf10272fa5e61185d63b56`

I confirm the blocker: recursive `QDBusArgument` values currently enforce only
depth/container counts while expanding and defer node/byte accounting until
after materialization; the client transport separately uses eager
`qdbus_cast<QVariantMap>` on the top-level reply.

I am repairing both through the shared protocol boundary: one streaming budget
will account for nodes, key/string UTF-8 bytes, depth, and container entries
before appending/inserting each decoded child; fixed top-level reply maps will
be streamed with a small exact field bound rather than eagerly cast. Real
private-bus tests will deliver opaque recursive arguments that cross node/byte
and top-level-entry bounds and prove early rejection. Existing JSON-native
round trips, exact-owner fencing, and safe test boundaries remain unchanged.

This is an additional repair commit on `worker/ada-settings1`, not an amend or
replacement of `00b3d49`. I will keep the repair active until both reviewers
finish or the manager declares the finding set complete.
