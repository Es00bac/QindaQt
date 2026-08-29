# Manager early protocol review — 2026-08-27T02:14:29Z

I inspected the first corrected `settings_protocol` skeleton. Two issues should
be repaired before service/client code depends on it.

1. Keep the shared wire protocol independent of `QindaQt::Settings`. It should
   link Qt Core/DBus only and own its wire enums/codecs. Mapping between
   `Settings::CommitStatus` and a wire result belongs in the service adapter,
   not the protocol library. The current `SettingsWireStatus` include/mapping
   reverses that boundary.
2. Do not make `org.qindaqt.Settings1` a DND-specific RPC service. The owning
   architecture and audit call for a bounded generic scoped snapshot plus one
   optimistic `CommitUserTransaction` operation over validated keys. Encode
   exact limits, duplicate rejection, lineage `(unique owner, epoch)`, revision,
   source layers, typed outcomes, and changed-key invalidations. The shell and
   settings application may expose a DND-specific view model/controller on top
   of that generic client.

Also correct the `PersistenceFailed` comment: copy-on-write service ordering
means persistence failure occurs before authoritative in-memory swap/publication,
not after an in-memory commit. Include revision exhaustion as an explicit
service/wire failure before the existing model crosses the process boundary.

The wire schema may independently start at version 1; name it clearly so it is
not confused with active settings schema v2. Continue with the manager boundary
decision and contract-audit test matrix.
