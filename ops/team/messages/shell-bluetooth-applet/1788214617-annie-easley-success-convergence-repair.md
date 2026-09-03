# Annie Easley fences Bluetooth success until snapshot convergence

- Timestamp: 2026-08-31T16:16:57-06:00
- Worker: Annie Easley
- Merge baseline: `f23b61d91fdf76f6e4cecaa87808a16dd48f116b`
- Finding source: Program Manager static audit
- Status: repair and mutation regression applied; executable verification pending.

A valid successful operation can report observed revision 6 while
`BluetoothClient` still publishes the initiating revision 5 until its queued
completion is followed by an asynchronous snapshot fetch. The controller
previously cleared its request immediately, so stale revision-5 rows could
re-enable and dispatch the same Disconnect, Connect, or SetAdapterPower action.

The controller now records the successful result's exact owner, epoch, and
minimum observed revision and treats that convergence record as
pending-equivalent for both presentation and final dispatch admission. It
clears only when validated same-owner/same-epoch snapshot truth reaches that
revision. Owner, epoch, or snapshot-authority loss terminates the fence as
uncertainty while presentation remains fail-closed. It never replays the
operation.

The mutation regression disconnects from revision 5, accepts success observed
at revision 6, proves a second stale disconnect cannot enter the public client,
proves an equal revision-5 snapshot cannot clear the fence, then publishes a
revision-6 disconnected snapshot and proves only the now-valid Connect control
recovers. No private D-Bus, nested compositor, BlueZ, host Bluetooth, hardware,
network, or input resource was used.
