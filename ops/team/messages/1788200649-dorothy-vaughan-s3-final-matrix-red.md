# S3 repaired full acceptance red and runtime release

- Worker: Dorothy Vaughan
- Update: 2026-08-31T12:24:09-06:00
- Preserved root: `/tmp/qindaqt-s3-selene-build`
- Lane: private-bus/private-nested-runtime terminally released

The one authorized command was:

```sh
QINDAQT_PRIVATE_RUNTIME_LANE=interactive-virtual-desktop \
ctest --test-dir /tmp/qindaqt-s3-selene-build --parallel 1 \
  --output-on-failure \
  -R '^(desktop\.virtual\.package-contract|desktop\.virtual\.interactive\.matrix\.(single-wuxga|single-1440p-125|single-1080p-150|dual-1080p-horizontal))$'
```

It exited 8 with 4/5 green:

- package contract: pass;
- WUXGA: pass, result `ac165c6aa1bfcf74039bbd7facac5ea3`;
- 1440p125: pass, result `84bb674959c53efe5605c14620296005`;
- 1080p150: fail, result `2c6dd8f5e1636b460207912945570edc`;
- dual: pass, result `2a0f47bf421465220987571e1372dffa`.

CTest parallelism was one, but the invocation did not include
`--stop-on-failure`; it automatically scheduled the dual row after the
1080p150 red before returning to the caller. No row was retried and no second
command was run. The overall acceptance remains red.

## First causal failure

The 1080p150 interaction returned exact code 8 and
`notification center did not map on the private seat`. Its preserved failure
capture shows:

- all required private services owned;
- one canonical 1280x720 logical WL-0 at generation 2;
- two shell PID 53 dock surfaces mapped and committed at settled 1280x30 and
  1280x34 geometry;
- Settings and Text Editor windows present on WL-0;
- no notification-center surface after the one target input batch.

The repaired probe returns code 10 when exact component/action press then
release is missing, out of order, or has the wrong identity. Reaching code 8
therefore proves that KGlobalAccel delivered exact
`qindaqt-shell/qindaqt_toggle_notification_center` press and release, but the
shell did not map the notification surface. The red disproves shortcut
delivery loss as the immediate remaining cause and localizes it after delivery
in the shell presentation/authority path. Because success markers are emitted
only with successful surface evidence, this failure has the precise return-code
proof rather than a misleading success marker.

The red result, sandbox command, four readiness probes, interaction log, exact
post-failure probe, and all service/compositor logs remain preserved. It failed
before PSS/capture acceptance, so no PSS or capture qualification is claimed
for that row. Fresh host process inspection finds no owned CTest, bwrap,
private bus, KWin, or QindaQt survivor.

I terminally release the serialized runtime lane. No retry, product edit,
commit, or freeze followed; manager classification is required.
