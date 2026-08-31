# S3 repaired 1440p125 consecutive green and runtime release

- Worker: Dorothy Vaughan
- Update: 2026-08-31T12:20:09-06:00
- Preserved root: `/tmp/qindaqt-s3-selene-build`
- Lane: private-bus/private-nested-runtime terminally released

The exact command was run twice consecutively:

```sh
QINDAQT_PRIVATE_RUNTIME_LANE=interactive-virtual-desktop \
ctest --test-dir /tmp/qindaqt-s3-selene-build --parallel 1 \
  --output-on-failure \
  -R '^desktop\.virtual\.interactive\.matrix\.single-1440p-125$'
```

Both invocations exited 0 and passed 2/2 because CTest automatically included
the package fixture; the requested 1440p125 row itself passed 1/1 in each
invocation. Preserved result IDs are:

1. `81c1656584fe4e246f84894a50de4963`
2. `9be7afb95baeb96710556598a4632ee2`

## Per-run evidence

Both interaction logs contain exactly the authenticated marker:

```text
QINDAQT_DESKTOP_NOTIFICATION_ACTIVATION={"action":"qindaqt_toggle_notification_center","component":"qindaqt-shell","pressed":true,"released":true}
```

Each run then proves one four-event Meta+N interaction with zero active center
before injection and one active, mapped, committed 440x640 notification center
owned by shell PID 53 on desired/actual output WL-0. The canonical output is
one 2048x1152 logical WL-0, rendered as 2560x1440 at requested scale 1.25 under
the unity-inspired/qinda-dusk scenario.

Containment is exact in both: host display, input, and session bus reachability
are all false; parent and child sockets differ; parent backend is
`kwin-virtual-qpaint` and QindaQt backend is `kwin-windowed-qpaint`.

Resource and capture evidence:

- `81c16565…`: 179,915 KiB PSS; 67,946-byte regular non-symlink PNG; full-frame
  SHA-256 `a91c4d4d4de7369f2ab36f84f7ddbd3d21340c8056b026acc4c3852cd03ebc08`.
- `9be7afb9…`: 179,936 KiB PSS; 68,066-byte regular non-symlink PNG; full-frame
  SHA-256 `1d0e4387744d0a4d5db382b516a52102198b82868c16572991bf18eb6a10bad9`.
- Both are exact 2560x1440, have 56 sampled frame colors, and record an exact
  550x801 interacted pixel region with 16 colors and SHA-256
  `1f021303452c588f353fab0165114f6503ab8072d76b5b4ee2adfb5255734531`.
- Both PSS values are below the 1,048,576 KiB ceiling.

Both cleanup records are bounded with empty `survivorPids`; fresh host process
inspection also finds no owned CTest, bwrap, private bus, KWin, or QindaQt role.
All result/log/capture directories remain preserved. I terminally release the
serialized private runtime lane. No full matrix has started; Program Manager
direction is required for any broader replay.
