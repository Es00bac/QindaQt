# Annie Easley Bluetooth B1 controller result-lineage red

- Timestamp: 2026-08-31T16:39:43-06:00
- Worker: Annie Easley
- Prior test alignment: `0099d8bfc9048e808d1957b2fa6639bf0363c851`
- Build root: `/tmp/qindaqt-bluetooth-b1-debug-5714b2f`
- Exact row: `qindaqt.bluetooth-applet-controller`
- Result: exit 8; 8 passed, 1 failed, 0 skipped in 15.12 seconds.

The controller target rebuilt 3/3. Its replay improved from 6/9 to 8/9; both
manager-requested mutation regressions and the two convergence-adjusted
lifecycles passed. The last close-path assertion correctly found the lease
retained: ReleaseDiscovery was initiated from validated revision 6, but the
test result helper still hard-coded initiating revision 5, so the controller
classified the nominal success uncertain.

The helper now requires an explicit initiating-revision value where the
operation starts after convergence. Release, cleanup, and the later Connect
fixture pass revision 6 explicitly. The wire-invalid `no-lease` fixture also
uses otherwise exact revision-6 lineage, isolating the intended wire mutation;
stale-result rejection remains covered in the request-state suite. Production
code is unchanged. The exact controller target and row will replay immediately.
No private D-Bus, display, BlueZ, host Bluetooth, hardware, network, or input
resource was used.
