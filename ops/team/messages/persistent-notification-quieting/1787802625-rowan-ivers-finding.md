# High: commit replies and invalidations are not fenced to the accepted lineage

Candidate `00b3d49ac3d7ba94edcf10272fa5e61185d63b56` authenticates the unique
sender but does not validate the rest of the promised `(owner, epoch,
revision)` commit lineage. In
`src/services/settings_client/src/settings_client.cpp:311-345`, commit decoding
never reads or matches `FieldEpoch` against the initiating snapshot, never
checks `revisionBefore` against the initiating base revision, never enforces
status-specific nonregressing `revisionAfter`, and accepts value/source maps
with arbitrary extra keys plus an unvalidated/unbounded `changedKeys` list. The
only requirements are parsable status/revisions/versions and presence of the
written key. Thus a stale/malformed same-owner reply can emit a confirmed
`commitFinished`, including `Applied`, even when it belongs to another epoch or
regresses/contradicts the request.

The invalidation path at `settings_client.cpp:247-267` likewise does not bound,
deduplicate, or validate changed keys. It trusts any future revision from the
current owner as `m_targetRevision`; a single maximum/far-future revision makes
every lower authoritative snapshot schedule an immediate follow-up forever at
`:301-308`. `QtSettingsTransport::handleSettingsChanged()` also relabels the
slot invocation with whichever owner is current at dispatch time
(`src/services/settings_client/src/qt_settings_transport.cpp:252-258`) rather
than carrying an observed sender/owner token into the callback.

This blocks the exact-owner/epoch/stale-callback contract and gives malformed
service traffic a resync-loop path. Focused tests exercise old-owner token
rejection and unsupported wire version, but not wrong/missing epoch, initiating
revision mismatch, status/revision contradictions, oversized/duplicate changed
keys, queued old-lineage signals, or far-future target revisions. Repair should
validate exact reply fields and status invariants against the stored initiating
request/snapshot, decode bounded changed keys, and fence invalidations with the
owner generation that installed the subscription.
