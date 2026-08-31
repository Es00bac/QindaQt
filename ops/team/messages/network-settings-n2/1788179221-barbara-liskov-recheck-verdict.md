# Barbara Liskov — Network Settings N2 repaired exact-commit recheck: ACCEPT

- Timestamp: 2026-08-31T06:27:01-06:00
- Reviewer: Barbara Liskov (independent exact-commit reviewer, different worker
  from the implementer; same reviewer who rejected `43b563c`)
- Exact candidate: `6f5d0ba9915851195a4776b3a1e2f224c369a958`
- Tree: `38bfff6a125735166d4bec9d56fb134646d0ea12`
- Sole parent: rejected immutable candidate
  `43b563cdfe08455269375e2c356112902e562641`
- Original base / merge base with `main`:
  `9b3d65542c87b2b977482ed7e72e4425c5332dd6`
- Review worktree:
  `/home/cabewse/work_SPaC3/container-wm-workers/network-settings-n2-liskov-review`
  (detached, exact, tracked content byte-clean)
- Verdict: **ACCEPT** — P0 0 / P1 0 / **P2 0** / P3 3
- Acceptance applies only to `6f5d0ba9915851195a4776b3a1e2f224c369a958`. I did
  not approve prose and no other SHA is covered.

## Summary

All four blocking P2 findings from my `43b563c` verdict reproduce as repaired
against this exact tree, by direct execution rather than by reading the repair
handoff. I found no P0, P1, or P2 defect in the repair itself, and no new
safety, correctness, secrecy, lineage, replay, lifetime, accessibility,
package, or documentation defect. The route still consumes only the public
Network client, protocol, and declared public model boundaries.

The repair's central move is sound: the previously private admission condition
in `beginOperation` is now one public read-only predicate,
`NetworkClient::operationAdmissionReady()`, and `beginOperation` is rewritten to
call it rather than restate it — so displayed availability and actual dispatch
cannot drift by construction. I checked every state transition that can change
an admission input and confirmed each one notifies:
`refresh`, `handleOwnerChanged`, `handleInvalidation`, `handleSnapshot`,
`handleOperation`, `handleBusDisconnected`, `requestSnapshotNow`,
`finishOperationAsUncertain`, `abortInFlight`, `scheduleRetry`, `publish`
(including its unchanged-state early return), `start`, and `stop`. The
reordering in `handleOwnerChanged` and `handleBusDisconnected` (clearing
`m_owner` before `abortInFlight`) and in `handleOperation` (scheduling the
authoritative refetch before publishing not-in-flight) exists specifically to
prevent a transient admissible window, and both timers are single-shot so the
predicate reopens normally.

## P0 findings

None.

## P1 findings

None.

## P2 findings

None. Direct reproductions of all four former P2 findings follow.

### P2-1 — repaired: projected availability now equals client admission

I built my own harness against this exact tree's Debug artifacts
(`/tmp/liskov-repro2`, deterministic fake public transport, no host contact)
and re-ran the exact sequence that failed at `43b563c`, asserting equality
between each projected availability flag and the real dispatch result.

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
P2-3      no BSSID/MAC in any projected AP or device row
PASS   : Repro::enabledActionsMatchAdmissionDuringSnapshotRefresh()
Totals: 3 passed, 0 failed, 0 skipped, 0 blacklisted
```

The state is identical to the one I rejected — still `Ready`, still not busy —
but the three controls now render disabled, every dispatch is refused, and the
transport operation count does not move. Admission reopens once the snapshot
settles and a real scan is then admitted. Both windows I named are covered: the
post-operation authoritative refresh and the invalidation refresh burst. The
model-verdict half is exact too — `scanAvailable()` and `requestScan()` share
the identical `kScanDeadlineMilliseconds` constant.

### P2-2 — repaired: documentation and rendered lineage now agree

```sh
grep -rn "serviceOwner\|serviceEpoch\|serviceRevision" src/apps/settings/network/qml/
# -> no matches (nor anywhere else in src/ outside the model itself)
```

`docs/wiki/apps/network-settings.md:21` no longer claims owner, epoch, and
revision are visible. It now states that connectivity and scan state are
visible, that the route model retains the exact owner/epoch/revision for
lineage gating and focused diagnostics, and that the ordinary page does not
render the broker's technical owner identifier. `NetworkPage.qml` binds
`connectivityText` and `scanStatusText`, so both halves of the sentence hold.

### P2-3 — repaired: no hardware address in the QML projection

The `bssid` key is gone from `accessPoints()`. Beyond checking that one key, my
harness scans every projected access-point and device row for any value
matching `^([0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}$` and finds none, so the page's
own "never exposes ... hardware addresses" sentence is now true. The committed
test also asserts key absence on every AP row.

### P2-4 — repaired: the gate now enforces ADR-0055's stated consequence

I re-ran hostile probes against the committed `check_boundary.cmake`
unmodified, using synthetic source roots. All five evasions I rejected
`43b563c` for are closed, and a valid closed surface still passes:

```
bare TextInput                                rejected
bare TextEdit                                 rejected
Controls TextArea (qualified T.TextArea)      rejected
renamed radio invokable enableWifiRadio(bool) rejected
credential invokable submitKey(QString)       rejected
multi-line Q_INVOKABLE                        rejected
Q_INVOKABLE with default-arg parenthesis      rejected
public Q_SLOTS: void setRadio(bool)           rejected
TextInput nested under qml/sub/               rejected
libnm include (control)                       rejected
real committed tree                           exit 0 (passes)
```

The C++ side is now a genuine closed set of exactly four permitted invokables
plus a blanket `Q_SLOT`/`Q_SLOTS` denial, rather than a five-name denylist, and
it fails closed on malformed input. `check_boundary_negative.cmake` proves the
permitted surface passes *before* proving each poison fails, which is the right
shape — it rules out a gate that rejects everything.

## P3 findings (non-blocking)

- **P3-A — the gate admits a writable `Q_PROPERTY`.** Reproduced directly:

  ```
  Q_PROPERTY(bool wifiEnabled READ wifiEnabled WRITE setWifiEnabled)  *** ACCEPTED ***
  ```

  A writable property is a third QML-reachable mutation channel that the closed
  -invokable assertion does not cover, so an idiomatic radio toggle
  (`Switch { onToggled: networkSettings.wifiEnabled = checked }`) could be added
  without the gate failing. Nothing is exposed at this SHA — all 21 properties
  are READ-only — and the new `stubMatchesRealModelSurface()` metaobject parity
  test independently catches a *newly added* property, so this is fenced in
  practice. A one-line fix is to deny `WRITE` inside `Q_PROPERTY` in the same
  scan. I judge this a guard-strength residual of the same family as P2-4, not a
  repeat of it: the specific repair I asked for landed and is verified.
- **P3-B — two QML scan-scope gaps.** Reproduced directly: a `.qml` placed at
  the route root instead of under `qml/` is never read by the credential check
  (`file(GLOB_RECURSE route_qml "${route_root}/qml/*.qml")`), and QML text
  embedded in a C++ string literal is not scanned. Both are ACCEPTED by the
  gate. Neither is reachable today: the QML module lists its five files
  explicitly, and `Qt6::Qml` was just removed from the static library's link, so
  inline QML instantiation would not link. Globbing `${route_root}/*.qml`
  recursively would close the first.
- **P3-C — commit message wrapping (former P3-5, partially repaired).** The
  literal `\n\n` escape sequences are gone and the body now has real paragraph
  breaks, but the two paragraphs remain single unwrapped lines of 286 and 283
  characters. Content is correct. Only a replacement commit could repair this
  and I do not think it is worth one.

## Former P3 findings: status at this SHA

- **P3-1 repaired** — the unused `hasInventory` QML property is removed and
  nothing in the repository references it.
- **P3-2 repaired** — the no-effect `m_model->knownNetworks[0].toMap();`
  statement is removed.
- **P3-3 repaired** — `supportsDocumentPagingKeys()` now covers Page Down,
  Ctrl+End, Page Up, and Ctrl+Home against the real `NetworkPage.qml` at
  420x320.
- **P3-4 repaired** — `QindaQt::NetworkModel` is now an explicit link and the
  unused `Qt6::Qml` link is removed from the static library; the QML module
  target keeps its own `Qt6::Qml`.
- **P3-5 partially repaired** — see P3-C.
- **P3-6 repaired** — `stubMatchesRealModelSurface()` asserts real/stub
  metaobject property and invokable surface equality, so a drift now fails.

## Verified as sound (no finding)

- Public-boundary discipline holds. The route's only non-Qt includes are
  `network_client`, `network_protocol`, and public `network_model` headers; no
  private service, adapter, transport, libnm, or Qt D-Bus reach-through, and no
  host radio or credential path. `CMakeLists.txt` links only
  `QindaQt::NetworkClient`, `QindaQt::NetworkModel`, `QindaQt::NetworkProtocol`,
  and `Qt6::Core`.
- Secrecy: `wireContainsSecrets` remains a second structural fence inside the
  rewritten `beginOperation`, and refusal text is mapped from stable reason
  codes to fixed translated strings by `actionFailureText`, so no diagnostic
  reaches `errorText` unredacted.
- Lineage and replay are unchanged in substance. Malformed and
  lineage-mismatched operation replies still route to
  `finishOperationAsUncertain`, which publishes `Degraded` and refetches rather
  than replaying. Owner replacement still clears the model behind the epoch
  high-water; the A→B→A fence is intact and the adversarial rows pass.
- No optimistic mutation: `handleOperation` still refetches authoritative truth
  instead of deriving state from the reply.
- Reload remains honest: `reloadAvailable()` is `!busy()`, and `reload()` can
  only fail when the transport cannot start — a real error worth surfacing — so
  it is not a second enabled-but-refused control.
- Accessibility: `firstFocusTarget` and `KeyNavigation.backtab` fall through to
  `reloadButton` when `scanButton` is disabled during a refresh, so the focus
  cycle stays closed. The `blockedReason` keys are diagnostic only and are not
  rendered, so a disabled control never shows an empty reason string.
- No leftover debug output, TODO, FIXME, or commented-out code in the diff.
- Every changed production file remains under the 500 non-blank-line
  decomposition threshold (`network_client.cpp` 467,
  `network_settings_model.cpp` 463, `network_client_admission.cpp` 57).

## Commands run, exit statuses, and counts

All builds and fixtures ran in external temporary roots outside the review
worktree. Host display, Wayland, session bus, NetworkManager, radio, and
credential state were kept out of the environment throughout
(`QT_QPA_PLATFORM=offscreen`, `DISPLAY`/`WAYLAND_DISPLAY` unset, both D-Bus
addresses pointed at nonexistent paths). No host NetworkManager, radio,
network, credential, or session bus was contacted.

- Provenance: `git rev-parse HEAD` = `6f5d0ba9915851195a4776b3a1e2f224c369a958`;
  `HEAD^{tree}` = `38bfff6a125735166d4bec9d56fb134646d0ea12`; `HEAD^` =
  `43b563cdfe08455269375e2c356112902e562641`, single parent;
  `git rev-list --count 43b563c..6f5d0ba` = 1; `git status --porcelain` empty.
- Repair diff scope: **16 changed paths, +502 / -66**. None under
  `tests/session/**`, Display implementation or tests, shell, `features.json`,
  `HANDOFF`, `TASK_LIST`, queues, or the private
  `network_service` / `network_manager_adapter` / `network_qt_transport` modules.
- `git diff --check 43b563c 6f5d0ba`: **exit 0**, no whitespace defects.
- Fresh configure, both external roots
  (`/tmp/qindaqt-liskov-recheck-{debug,release}`, removed and recreated):
  `QINDAQT_ENABLE_STRICT_WARNINGS=ON`, `BUILD_TESTING=ON`,
  KWin-plugin/shell/production-shell OFF,
  `CMAKE_PREFIX_PATH=/tmp/qindaqt-kdecoration-prefix/usr`. **exit 0** each.
- Fresh builds, both roots: **exit 0** each, 1971 targets,
  **0 compiler warnings, 0 errors** in both logs.
- Mutation-focused 5-row proof
  (`network-client-admission`, `network-settings-model`, `network-page`,
  `network-settings-boundary`, `network-settings-boundary-poison`),
  `--no-tests=error --parallel 1`: **5/5 passed, 0 failed, exit 0** in Debug
  and in Release.
- Complete 14-row Network and Settings selector, both profiles:
  **14/14 passed, 0 failed, exit 0** in Debug (25.55 s) and Release (25.11 s).
- Direct public-client package policy 5-row selector
  (`network-client`, `network-client-admission`,
  `network-installed-header-consumer`, `network-boundary`,
  `network-boundary-poison`): **5/5 passed, exit 0** in Debug and Release.
- Broader sweep beyond the requested selectors, because this repair changes the
  shared public client that `network_qt_transport` and the Network service
  tests also consume: `-L network` **25/25 passed, exit 0** and `-L settings`
  **9/9 passed, exit 0**, in both configurations.
- Direct hostile probes against the committed `check_boundary.cmake`:
  **13 probes — 10 correctly rejected, 3 accepted** (the two residual classes in
  P3-A and P3-B); the real committed tree passes at exit 0.
- Direct P2-1/P2-3 reproduction: 1 purpose-built harness, **3 passed, 0 failed**,
  covering both refresh windows and the MAC-shape scan.
- `tools/validate-docs`: **exit 0**, 112 Markdown documents and MkDocs
  navigation.
- `/tmp/qindaqt-display-repair2-docs-venv/bin/mkdocs build --strict`: **exit 0**.
- `tools/check-source-shape`: **exit 0**, only the same three pre-existing
  unrelated warnings (`tst_color_model.cpp`, two `tests/session` Python files)
  and no new size warning.
- Residue and cleanliness at review close: `git status --porcelain` empty and
  `git clean -nxd` reports only this message directory and my own worker
  record, after I removed the `__pycache__` directories my tool runs created.
  The worktree is still exactly at `6f5d0ba` / tree `38bfff6a`.

## Current-main merge conflicts

`git merge-tree --write-tree ab203cac213b4bff882151de2398b4c1b46c99cb 6f5d0ba`
exits 1 with **content conflicts in `docs/wiki/adr/index.md` and `mkdocs.yml`**.
The base `9b3d655` is a clean ancestor of `main` (main is 16 commits ahead).
Both conflicts are adjacent additive appends at the same table and nav tail:

- `docs/wiki/adr/index.md` — `main` appends the ADR-0053 row; the candidate
  appends the ADR-0055 row.
- `mkdocs.yml` — `main` appends the ADR-0053 nav entry; the candidate appends
  the Settings Network route page and the ADR-0055 nav entry.

Resolution is mechanical: keep both, in numeric order (0053 then 0055). This is
ordinary shared-registry churn, not a defect in the candidate, but the manager
must resolve it at integration. ADR-0054 remains a gap consistent with the
index's reserved-number policy; I did not treat it as a finding.

## Remaining bounded caveats

- The evidence class is unchanged from my first review and from Radia's:
  deterministic fake-public-transport, absent-private-bus, offscreen
  software-renderer, source-boundary poison, and relocated-package coverage. I
  make no claim about physical Wi-Fi, Ethernet, radios, host NetworkManager
  mutation, stored-profile compatibility, external secret-agent qualification,
  persistence, shell applets, or session runtime.
- Behavioural consequence worth stating plainly: under a pathological service
  that invalidates faster than the client can refetch, admission stays closed
  and Scan/Connect/Disconnect remain disabled with no user-visible "refreshing"
  explanation. This is fail-closed, is now documented at
  `docs/wiki/apps/network-settings.md:44`, and is strictly better than the
  previous enabled-but-refused behavior. Observation, not a finding.
- Cross-module note for the manager: the repair edits
  `src/services/network_client/**`, which is the Network N1 module rather than
  the Settings route. My original P2-1 asked for coordination with the N0/N1
  owner. The Platform queue shows QQ-005.04 already integrated with no live
  owner, so there was no live owner to coordinate with. The change is purely
  additive to the public header (one predicate, one signal, one private flag),
  is documented in that module's own normative architecture page
  (`docs/wiki/architecture/network-service.md:60`), and is covered by a new
  focused test in that module's own test directory
  (`qindaqt.network-client-admission`). I record it as a fact for integration,
  not as a defect.
- One residual asymmetry in `handleSnapshot`: moving the `m_dirty` reschedule
  above decoding means that when a malformed snapshot arrives while dirty,
  `scheduleRetry()`'s backoff now overwrites the immediate refetch rather than
  the reverse. `m_dirty` is consumed either way and the refetch still happens,
  so this is a benign — arguably better — change. Not a finding.
- I did not edit, amend, or commit any tracked candidate file. The only files I
  created are the three messages in this thread and
  `ops/team/workers/barbara-liskov-network-settings-review.md`.

## Requested next action

Route exact commit `6f5d0ba9915851195a4776b3a1e2f224c369a958` to the Program
Manager for integration, resolving the two additive
`docs/wiki/adr/index.md` / `mkdocs.yml` conflicts against
`ab203cac213b4bff882151de2398b4c1b46c99cb` by keeping both rows in numeric
order, then rerunning the affected gates on the integrated tree. There is no
blocking reproduction to return to Radia Perlman.

The three P3 items are non-blocking and need not gate integration. If the
workgroup wants them closed, P3-A is a one-line `WRITE` denial and P3-B a
one-line glob widening in `tests/apps/settings/network/check_boundary.cmake`;
both belong to a later Network Settings outcome, not to this candidate.

I own no further work on this candidate. **The serialized compiler, CTest, and
private-bus lane is terminal and released** as of this verdict.
