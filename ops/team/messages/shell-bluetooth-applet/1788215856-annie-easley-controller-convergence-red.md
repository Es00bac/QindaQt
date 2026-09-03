# Annie Easley Bluetooth B1 controller convergence red

- Timestamp: 2026-08-31T16:37:36-06:00
- Worker: Annie Easley
- Compiled source head: `1cbedecf1a6dad96353e7079559864442c35b22e`
- Build root: `/tmp/qindaqt-bluetooth-b1-debug-5714b2f`
- Exact row: `qindaqt.bluetooth-applet-controller`
- Result: exit 8; 6 passed, 3 failed, 0 skipped in 15.11 seconds.

All requested Debug targets compiled and linked after the five preserved strict
initializer repairs. The controller row stopped before any broader selector.
The new stale-success duplicate-dispatch regression passed. Three pre-existing
lease tests failed because they still assumed a successful AcquireDiscovery
result observed at revision 6 made the initiating revision-5 snapshot current:
close did not yet dispatch release, explicit release remained fenced, and the
owner-replacement setup could not dispatch its next operation.

The repair is test-only. Each affected fixture now publishes validated
same-owner/epoch revision-6 `discovering=true` truth before release or another
mutation. The close fixture additionally publishes revision-7
`discovering=false` truth after release success before expecting the
pending-equivalent fence to clear. This aligns older tests with the accepted
product contract without weakening the fence. The exact controller target and
row will replay before broader CTest. No private D-Bus, display, BlueZ, host
Bluetooth, hardware, network, or input resource was used.
