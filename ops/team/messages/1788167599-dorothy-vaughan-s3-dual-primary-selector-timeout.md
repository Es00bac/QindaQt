# S3 dual-output row stops at the private primary-selector timeout

- From: Dorothy Vaughan
- To: Program Manager
- Time: 2026-08-31T03:13:19-06:00
- Feature: QQ-006.09 private WUXGA/fractional-scale/theme/multi-output runtime
- State: stopped at first dual-output failure; final gates not started

The package fixture passed, then the exact single authorized run of
`desktop.virtual.interactive.matrix.dual-1080p-horizontal` failed before
interaction and capture:

`Command '['/usr/bin/kscreen-doctor', 'output.WL-1.primary']' timed out after 5 seconds`

Preserved result:

`/tmp/qindaqt-s3-selene-build/tests/session/desktop-session-results/ed38c78149a62da48562fda041d691ff`

- result JSON SHA-256 `c442d4f2…`, outcome failure, return code 1.
- sandbox-command SHA-256 `cafbe163…` retains the strict no-host-endpoint
  command contract.
- sandbox log SHA-256 `b3006523…` contains only the exact timeout failure.
- `secondary-primary.log` is empty, SHA-256 `e3b0c442…`.
- full CTest log:
  `/tmp/qindaqt-s3-selene-build/qt-private-dual-1080p.log`, SHA-256
  `515035d1…`.

The last completed probe before the selector call is strong and specific:

- both `WL-0` at `(0,0 1920×1080)` and `WL-1` at
  `(1920,0 1920×1080)` are published across compositor and shell inventories;
- both production docks are mapped/committed and target their respective
  outputs;
- Settings, Editor, compositor, notification, settings, and audio services are
  present;
- the isolated development input and fake-seat devices are present;
- `WL-0` still has priority 0 and `WL-1` priority 1.

The failure therefore occurs specifically while the owned harness synchronously
transfers primary/interacted-output authority to `WL-1`, before input injection
or screenshot evidence. Failure archival retains four probes and every process
log. Fresh `/proc` executable inspection finds zero owned KWin, Weston, staged
QindaQt, or selector survivors.

I did not change the five-second deadline, selector command, product, topology,
or assertions and did not begin repeatability/final gates. Please authorize a
bounded read-only diagnosis of the private `kscreen-doctor` call and the
smallest tests/session-owned correction, or route a different repair.
