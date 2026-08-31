# Barbara Liskov — Network Settings N2 recheck midpoint: all four P2 findings reproduce as repaired

- Timestamp: 2026-08-31T06:24:16-06:00
- Exact candidate: `6f5d0ba9915851195a4776b3a1e2f224c369a958`
- Tree: `38bfff6a125735166d4bec9d56fb134646d0ea12`
- Status: recheck in progress; no blocking finding so far.

## Direct reproductions of the four former P2 findings

**P2-1 — repaired, reproduced directly.** I built my own harness against this
exact tree's Debug artifacts (`/tmp/liskov-repro2`, deterministic fake public
transport, no host contact) and re-ran the exact sequence that failed at
`43b563c`. In the same state that previously projected enabled-but-refused
controls — still `Ready`, still not busy — availability now matches admission
exactly, and nothing is dispatched:

```
WINDOW A  state ready: true   busy: false
WINDOW A  scanAvailable: false
WINDOW A  knownNetworks[1].connectAvailable: false
WINDOW A  devices[0].disconnectAvailable: false
WINDOW A  requestScan() returned: false   errorText: "The network service is not ready."
WINDOW A  connectKnownNetwork() returned: false
WINDOW A  disconnectDevice() returned: false
AFTER A   admission reopened, scanAvailable: true
WINDOW B  scanAvailable immediately after invalidation: false
WINDOW B  scanAvailable while refetch outstanding: false
PASS   : Repro::enabledActionsMatchAdmissionDuringSnapshotRefresh()
```

The harness asserts equality between each projected availability flag and the
actual dispatch result, and that the transport operation count does not move.
Both the post-operation refresh window and the invalidation refresh burst are
covered. `scanAvailable()` and `requestScan()` share the identical
`kScanDeadlineMilliseconds` constant, so the model verdict half is exact too.

**P2-2 — repaired.** No QML in the repository references `serviceOwner`,
`serviceEpoch`, or `serviceRevision`. The page renders `connectivityText` and
`scanStatusText`, and `docs/wiki/apps/network-settings.md:21` no longer claims
owner/epoch/revision are visible; it now states the model retains them for
lineage gating and focused diagnostics and that the ordinary page does not
render the broker's technical owner identifier. Documentation and code agree.

**P2-3 — repaired.** The `bssid` key is gone from the access-point projection.
My harness additionally scans every projected access-point and device row for
any `xx:xx:xx:xx:xx:xx`-shaped value and finds none, so the page's own
"never exposes ... hardware addresses" sentence is now true.

**P2-4 — repaired.** I re-ran my hostile probes against the committed
`check_boundary.cmake` unmodified. All five evasions I rejected `43b563c` for
are now closed, and a valid closed surface still passes:

```
bare TextInput                      rejected
bare TextEdit                       rejected
Controls TextArea (qualified)       rejected
renamed radio invokable             rejected
credential invokable submitKey      rejected
multi-line Q_INVOKABLE              rejected
Q_INVOKABLE w/ default-arg paren    rejected
public Q_SLOTS setRadio             rejected
TextInput nested under qml/sub/     rejected
libnm include (control)             rejected
real committed tree                 exit 0 (passes)
```

The C++ side is now a genuine closed set of exactly four permitted invokables
rather than a name denylist, and `check_boundary_negative.cmake` proves the
permitted surface passes before proving each poison fails.

## Material finding: two residual gate gaps (non-blocking)

Two hostile probes still pass, and I am recording them rather than blocking:

```
Q_PROPERTY(bool wifiEnabled READ ... WRITE setWifiEnabled)  *** ACCEPTED ***
TextField text inside a C++ string literal in a .cpp        *** ACCEPTED ***
.qml placed at route root instead of under qml/             *** ACCEPTED ***
```

A writable `Q_PROPERTY` is a third QML-reachable mutation channel that the
closed-invokable assertion does not cover, so an idiomatic radio toggle could
still be added without the gate failing. Nothing is exposed at this SHA — every
property is READ-only — and the new `stubMatchesRealModelSurface()` metaobject
parity test independently catches a *newly added* property. I judge this a
guard-strength residual of the same family as P2-4, not a repeat of it: the
specific repair I asked for landed and is verified.

## Gates so far

- Fresh strict Debug and Release configure + build from clean external roots:
  exit 0 each, **0 compiler warnings, 0 errors** in both logs.
- Mutation-focused 5-row proof: 5/5, exit 0, Debug and Release.
- Complete 14-row Network and Settings selector: 14/14, exit 0, Debug (25.55 s)
  and Release (25.11 s).
- Direct public-client package policy 5-row selector: 5/5, exit 0, both.
- Broader sweep beyond the requested selectors, because this repair changes the
  shared public client that `network_qt_transport` and the Network service
  tests also consume: `-L network` 25/25 and `-L settings` 9/9, exit 0, in both
  configurations.
- `tools/validate-docs` exit 0 (112 documents); pinned strict MkDocs exit 0;
  `tools/check-source-shape` exit 0 with only the same three pre-existing
  unrelated warnings and no new size warning; `git diff --check` exit 0.

Host display, Wayland, session bus, NetworkManager, radio, and credential state
were kept out of the environment throughout. Remaining: final new-defect sweep
and the terminal verdict.
