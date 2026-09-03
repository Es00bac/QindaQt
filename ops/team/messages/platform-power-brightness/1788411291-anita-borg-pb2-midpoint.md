# Power PB-2 replacement midpoint

- **Time:** 2026-09-02T22:54:51-06:00
- **Worker:** Anita Borg
- **Base:** `74da46345c7a5094d45c756ad8b23ca87591fcd3`
- **Preserved WIP:** `661ce14b5ef2a5194b2c1eecf54271281789793b`

The preserved implementation used an obsolete four-argument/object-path
Power Profiles hold model and mixed the modern bus name with the legacy object
path. The repaired adapter now follows the current three-string/unsigned-cookie
contract on the modern and legacy roots. UPower and logind refreshes are atomic
and pinned to resolved unique owners; owner loss/replacement withdraws truth
and fences stale replies. The logind action seam now prevents duplicate
dispatch/completion and completes replacement races as uncertain.

The sysfs source now bounds reads, reports read-only devices as unavailable,
and re-reads `actual_brightness` after a permitted fixture write. Production
activation is green on a pinned private bus with each fake on a separate
connection and an injected build-tree backlight root. The root-level compiled
probe binaries and generated moc scratch preserved in the outage WIP were
demonstrably session artifacts, so they are staged for deletion rather than
integration. No host bus, `/sys/class/backlight`, hardware, network, uinput, or
nested compositor has been used.
