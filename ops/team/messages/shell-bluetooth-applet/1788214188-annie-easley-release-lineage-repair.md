# Annie Easley repairs Bluetooth discovery-release lineage

- Timestamp: 2026-08-31T16:09:48-06:00
- Worker: Annie Easley
- Merge baseline: `f23b61d91fdf76f6e4cecaa87808a16dd48f116b`
- Finding source: Program Manager static audit
- Status: repair and mutation regression applied; executable verification pending.

`BluetoothAppletController::handleOperationCompleted` cleared the tracked
discovery lease whenever a non-successful ReleaseDiscovery completion carried
raw reason text `no-lease`. That branch ran after `applyBluetoothResult`, so a
wire-invalid or wrong-lineage completion could be classified Uncertain yet
still obtain lease-lifetime authority from an unvalidated string. The applet
would then lose its only release target even though neither validated success
nor current snapshot truth proved the lease ended.

The repair removes the raw-reason special case. Failed, uncertain, malformed,
and stale releases retain the lease without replay; existing
`retireLeaseIfAuthorityEnded()` remains the single path for current
owner/snapshot proof, while exact validated ReleaseDiscovery success remains
the completion path. The controller test now acquires a real tracked lease,
submits release, injects a matching wire-invalid `no-lease` completion at the
controller's final admission boundary, and requires Uncertain feedback plus a
retained lease. The owning Bluetooth applet contract states the same rule.

No BlueZ, private D-Bus, host Bluetooth, hardware, nested compositor, network,
or input resource was contacted. The exact focused row and all required
Debug/Release adjacent gates remain to be executed before candidate freeze.
