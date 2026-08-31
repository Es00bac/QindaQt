# Barbara Liskov — Network Settings N2 exact-commit review: REJECT

- Timestamp: 2026-08-31T04:50:22-06:00
- Reviewer: Barbara Liskov (independent exact-commit reviewer, different worker
  from the implementer)
- Exact candidate: `43b563cdfe08455269375e2c356112902e562641`
- Tree: `31ec5f7acb312bd2f53884351fc6fdaa1b84dd5f`
- Sole parent / merge base: `9b3d65542c87b2b977482ed7e72e4425c5332dd6`
- Review worktree: `/home/cabewse/work_SPaC3/container-wm-workers/network-settings-n2-liskov-review`
  (detached, exact, tracked content byte-clean)
- Verdict: **REJECT** — P0 0 / P1 0 / **P2 4** / P3 6
- Acceptance, if later granted, applies only to one exact SHA. This review
  applies only to `43b563cdfe08455269375e2c356112902e562641`.

## Summary

The functional core of this candidate is sound and I found no safety, secrecy,
lineage, or replay defect. Public-boundary discipline is real: the route links
`QindaQt::NetworkClient` / `QindaQt::NetworkProtocol` only, includes no private
service, adapter, transport, libnm, or Qt D-Bus header, exposes no radio or
credential mutation invokable, never manufactures inventory from an operation
reply, and never replays an uncertain outcome. Exact owner/epoch/revision
retirement — including the A→B→A sequence — clears retired inventory and holds
the lineage high-water. All fourteen requested rows pass in freshly configured
strict Debug and strict Release with zero compiler warnings, and every static
gate passes.

I am nonetheless rejecting. Four P2 findings are open, and three of them are
self-contradictions introduced by this same commit: its own normative wiki page
asserts two user-visible/security-boundary behaviors the code does not have,
and its own ADR-0055 records an accepted consequence that its own boundary gate
does not enforce. `AGENTS.md` states that changes leaving documentation or
accepted ADR consequences inaccurate are incomplete. Each repair is small and
local. None of the four is a boundary breach or a state-corruption risk.

## P0 findings

None.

## P1 findings

None.

## P2 findings (blocking)

### P2-1 — Enabled Scan/Connect/Disconnect are refused during an in-flight snapshot refresh

`src/apps/settings/network/network_settings_model.cpp:182` derives `busy()`
solely from `NetworkClient::operationInFlight()`, which reflects only
`m_operation`. `NetworkClient::beginOperation`
(`src/services/network_client/src/network_client.cpp:425`) additionally requires
`!m_request`. The client issues an authoritative snapshot refresh after *every*
successful operation (`network_client.cpp:364`) and on every service
invalidation (`network_client.cpp:287`). During that window the state is still
`Ready`, `busy()` is `false`, and the intent verdicts still evaluate against the
last accepted snapshot, so `scanAvailable()` (`:188`), each row's
`disconnectAvailable` (`:300`) and `connectAvailable` (`:353`) all project
`true` and the controls render enabled — but every dispatch is refused and the
user is shown `"The network service is not ready."`

Exact reproduction (built against this exact tree's Debug artifacts,
`/tmp/liskov-repro`, deterministic fake public transport, no host contact):

```
QINFO : state ready: true  busy: false
QINFO : scanAvailable (drives Scan button enabled): true
QINFO : knownNetworks[1].connectAvailable: true
QINFO : devices[0].disconnectAvailable: true
QINFO : requestScan() returned: false        errorText: "The network service is not ready."
QINFO : connectKnownNetwork() returned: false errorText: "The network service is not ready."
QINFO : disconnectDevice() returned: false    errorText: "The network service is not ready."
PASS  : Repro::enabledActionsRejectDuringInFlightSnapshotRefresh()
```

The sequence is: reach Ready, dispatch an admitted scan, let the operation
succeed while the follow-on snapshot reply is still outstanding. On a live bus
this is the state immediately after every successful scan/connect/disconnect
and during every invalidation burst.

Behavior fails closed — no mutation is dispatched and no state is corrupted —
so this is a correctness/UX defect, not a safety one. Note the repair is not
purely local: the public `NetworkClient` exposes no request-in-flight
predicate, so either the client needs a small public addition (coordinate with
the Network N0/N1 owner) or the route must treat a transient refusal as a
non-error rather than surfacing "not ready".

### P2-2 — The wiki asserts owner/epoch/revision are visible; the page renders none of them

`docs/wiki/apps/network-settings.md:21` states "Connectivity, owner, epoch,
revision, and scan state remain visible alongside the inventory." The model does
expose `serviceOwner`, `serviceEpoch`, and `serviceRevision`
(`network_settings_model.h:34-36`), but no QML file references any of the three:

```sh
grep -rn "serviceOwner\|serviceEpoch\|serviceRevision" \
  src/apps/settings/network/qml/ src/apps/settings_center/*.qml
# -> no matches
```

`NetworkPage.qml` renders only `connectivityText`, `statusText`,
`operationStatusText`, `errorText`, and `scanStatusText`. Either surface the
lineage triple or correct the sentence.

### P2-3 — The route publishes an access point hardware address while the same page says it never does

`docs/wiki/apps/network-settings.md:24` states the route "never exposes
NetworkManager object paths, hardware addresses, private service values, or
unbounded backend diagnostics." `network_settings_model.cpp:324` puts
`{"bssid", point.bssid}` into every QML-visible access-point row.

That value is the literal AP MAC: `libnm_network_manager_facts.cpp:201` reads
`nm_access_point_get_bssid()`, and `network_identity.cpp:89` `normalizeBssid`
only validates the `xx:xx:xx:xx:xx:xx` shape and lowercases it — it is not a
derived pseudonym (the pseudonym in this protocol is `knownNetworkId`). No QML
in the module consumes the key, so it is an unused widening of the projection
that directly contradicts the change's own boundary sentence. Drop the key or
amend the doc.

### P2-4 — The boundary poison gate does not deny the credential/radio surfaces ADR-0055 says it must

ADR-0055 Consequences (`docs/wiki/adr/0055-...md:65-67`) records as accepted:
"Package and source checks must fail if the route gains a private service
dependency, direct D-Bus/libnm access, credential input, or a radio mutation
entry point." The gate does not meet that for the ordinary alternatives.

`tests/apps/settings/network/check_boundary.cmake:26` denies only `TextField`
among entry controls. I ran the committed script unmodified against synthetic
source roots:

```
PROBE 'bare TextInput (secret-capable, no trigger words)': *** ACCEPTED — guard did not catch ***
PROBE 'bare TextEdit':                                     *** ACCEPTED — guard did not catch ***
PROBE 'Controls TextArea':                                 *** ACCEPTED — guard did not catch ***
PROBE 'control baseline: private service include':         rejected
```

`TextInput`, `TextEdit`, and `TextArea` are all free-text entry controls fully
capable of collecting a passphrase. Separately, `check_boundary.cmake:17` is a
five-name denylist, so a renamed entry point passes:

```
PROBE 'renamed radio mutation invokable' (Q_INVOKABLE void enableWifiRadio(bool)): *** ACCEPTED ***
PROBE 'credential invokable named submitKey' (Q_INVOKABLE void submitKey(QString)): *** ACCEPTED ***
```

The shipped route contains no such control, so nothing is presently exposed —
this is a guard-strength defect against the ADR's own stated consequence.
Widening the QML regex to the text-entry family and, for the C++ side,
asserting a closed invokable set rather than enumerating forbidden names would
close it.

## P3 findings (non-blocking)

- **P3-1** `src/apps/settings/network/qml/NetworkPage.qml:16` declares
  `readonly property bool hasInventory` which is referenced nowhere in the
  repository. Dead code.
- **P3-2** `tests/apps/settings/network/tst_network_page.cpp:168`
  `m_model->knownNetworks[0].toMap();` is a statement with no effect; its result
  is discarded and the following line recomputes it. Leftover.
- **P3-3** The Page Up / Page Down / Ctrl+Home / Ctrl+End behavior documented at
  `docs/wiki/apps/network-settings.md:67-68` has no committed test. I verified
  it does work at 420x320 (`contentY` 0 → 30 on Page Down, → 525 on Ctrl+End,
  → 0 on Ctrl+Home), so this is an unguarded regression surface, not a defect.
- **P3-4** `src/apps/settings/network/CMakeLists.txt:16` links
  `QindaQt::NetworkClient QindaQt::NetworkProtocol`, but
  `network_settings_model.cpp:5-6` directly includes
  `network_model/network_intent_policy.h` and `network_model/network_model_state.h`.
  It compiles only through `NetworkClient`'s PUBLIC transitive link. Declare
  `QindaQt::NetworkModel` explicitly. (`Qt6::Qml` also appears unused by the
  static library — the header needs only QtCore for moc.)
- **P3-5** The commit message body is a single unwrapped ~1,100-character line
  carrying literal `\n\n` escape sequences instead of paragraph breaks. Content
  is correct; only a replacement commit can repair the formatting.
- **P3-6** `stub_network_settings_model.h` duplicates the entire real property
  surface, and `NetworkPage.qml` is only ever exercised against the stub;
  `check_route_construction.cmake:57` proves process residency, not QML binding
  health, so a real-model/QML property drift would pass every gate. I confirmed
  no drift exists at this SHA — the model, stub, and QML-referenced property
  sets are identical. A meta-object equivalence assertion would keep it that way.

## Verified as sound (no finding)

- Public-boundary discipline: the route's only non-Qt includes are
  `network_client`, `network_protocol`, and public `network_model` headers; no
  D-Bus, libnm, private service, adapter, or transport reach-through.
- Exact lineage retirement including A→B→A: retired epoch 20/rev 99 rejected
  behind high-water epoch 21, inventory cleared, owner emptied.
- No optimistic mutation: `handleOperationFinished` only sets status text; the
  client refreshes rather than deriving state from the reply
  (`network_client.cpp:361-364`).
- No replay: mismatched operation lineage and malformed replies both route to
  `finishOperationAsUncertain`; the model surfaces it and never re-dispatches.
- Capability and busy admission for the *operation* case: capability removal
  disables and refuses scan/connect/disconnect.
- Secret redaction: `AccessPoint`/`KnownNetwork` structurally cannot carry a
  secret; the transport diagnostic `password="do not publish"` does not reach
  `errorText`; no row key matches password/passphrase/privateKey.
- No credential or radio mutation path: no `setRadio`/secret invokable on the
  model, `credentialEntrySupported` is a `constexpr false`, and no text-entry
  control exists in any of the five QML files.
- `NetworkClient::start()` is idempotent (`network_client.cpp:112-116`), so the
  page's Reload works after `main.cpp` has already started the client, and
  rolls back cleanly on transport failure so Retry can re-start.
- Closed route registry and CLI: `main.cpp:95` rejects an unknown `--page` with
  exit 2 before any transport, client, or model is constructed, confirming the
  ADR-0055 ordering claim.
- Installed relocation: the staged-prefix row withholds the installed Network
  module while the developer tree remains present, requires exit 3, restores,
  and proves all four routes from the relocated prefix.
- Object lifetime in `main.cpp:169-172`: transport, client, and model are
  declared in dependency order, so destruction order is safe.
- The `tst_settings_navigation_page.cpp` Alt+Left expectation change
  (appearance → notifications) is a correct consequence of inserting the Ctrl+4
  step into the history, not a regression.

## Commands run and counts

All builds and fixtures ran outside the review worktree. No host
NetworkManager, radio, network, credential, or session bus was contacted.

- Provenance: `git rev-parse HEAD` = `43b563cdfe08455269375e2c356112902e562641`;
  `HEAD^{tree}` = `31ec5f7acb312bd2f53884351fc6fdaa1b84dd5f`; `HEAD^` =
  `9b3d65542c87b2b977482ed7e72e4425c5332dd6`; `git merge-base` = same;
  `git rev-list --count 9b3d655..HEAD` = 1; single parent.
- Fresh configure, both roots: `QINDAQT_ENABLE_STRICT_WARNINGS=ON`,
  `BUILD_TESTING=ON`, KWin-plugin/shell/production-shell OFF,
  `CMAKE_PREFIX_PATH=/tmp/qindaqt-kdecoration-prefix/usr`. Exit 0 each.
- Fresh builds `/tmp/qindaqt-liskov-network-{debug,release}`: exit 0 each,
  **0 warnings, 0 errors** in both logs.
- Focused 14-row selector, both roots, `--no-tests=error --parallel 1`:
  **14/14 passed, 0 failed, exit 0** in Debug (25.53 s) and Release (25.05 s).
  Includes `network-settings-model`, `network-settings-model-adversarial`,
  `network-page`, `network-settings-boundary`,
  `network-settings-boundary-poison`, `settings-route-registry`,
  `settings-navigation-controller`, `settings-navigation-page`,
  `settings-app-offscreen`, `settings-app-rejects-unknown-route`,
  `settings-app-rejects-missing-theme`, `settings-app-desktop-identity`,
  `settings-app-route-construction`, `settings-app-installed-routes`.
- Direct hostile probes against the committed `check_boundary.cmake`: 8 probes,
  5 evasions accepted (see P2-4), 3 controls correctly rejected.
- Direct busy-admission reproduction: 1 executable harness, reproduced 1/1
  (see P2-1).
- Direct keyboard-paging verification: 1 harness against the real
  `NetworkPage.qml`, documented behavior confirmed working (see P3-3).
- Property-surface cross-check: QML-referenced ∖ model = ∅; model ∖ stub = ∅;
  stub ∖ model = ∅.
- `tools/validate-docs`: exit 0, 112 Markdown documents and MkDocs navigation.
- `mkdocs build --strict`: exit 0.
- `tools/check-source-shape`: exit 0, 1678 files; only three pre-existing
  unrelated warnings (`tst_color_model.cpp`, two `tests/session` Python files).
  Largest new production source `network_settings_model.cpp` = 461 non-blank
  lines, under the 500 review threshold.
- `git diff --check 9b3d655 HEAD`: exit 0, no whitespace defects.
- Residue and cleanliness: `git status --porcelain` empty and `git clean -nxd`
  empty at review close, after removing the `__pycache__` directories my own
  tool runs created.
- Changed-path scope confirmed: 38 paths, none under `tests/session/**`,
  Display implementation/tests, shell, `features.json`, `HANDOFF`,
  `TASK_LIST`, queues, or private Network platform modules.

## Caveats

- `git merge-tree --write-tree main 43b563c` reports **content conflicts in
  `docs/wiki/adr/index.md` and `mkdocs.yml`**. The base `9b3d655` is a clean
  ancestor of `main` (main is 9 commits ahead), and both conflicts are adjacent
  additive appends: `main` added the ADR-0053 rows at the same table and nav
  tail where this candidate appends ADR-0055. Resolution is mechanical — keep
  both rows in numeric order. This is ordinary shared-registry churn, not a
  defect in the candidate, but the manager must resolve it at integration.
- ADR-0054 is a gap between `main`'s 0053 and this candidate's 0055,
  consistent with the index's reserved-number policy; I did not treat it as a
  finding.
- My evidence is the same class Radia Perlman declared: deterministic
  fake-public-transport, absent-private-bus, offscreen software-renderer, and
  relocated-package coverage. I make no claim about physical Wi-Fi, Ethernet,
  radios, host NetworkManager mutation, stored-profile compatibility, external
  secret-agent qualification, persistence, shell applets, or session runtime.
- Observation, not a finding: at 420x320 the scrollable form viewport is only
  46 px tall because the heading, service card, operation label, error label,
  and the permanently pinned credential-boundary card sit above it. Content
  remains reachable by scrolling and this matches the Display/Appearance page
  structure.
- I did not edit, amend, or commit any product source. The only files I created
  are this message and
  `ops/team/workers/barbara-liskov-network-settings-review.md`.

## Requested next action

Return this exact blocking reproduction to Radia Perlman in
`/home/cabewse/work_SPaC3/container-wm-workers/network-settings-n2` for repair
in the same worktree, then route the repaired commit back for a different-worker
exact recheck. The four P2 repairs are:

1. Make the projected availability of scan/connect/disconnect agree with what
   the client will actually admit while a snapshot request is outstanding, or
   stop surfacing that transient refusal as a "not ready" error (P2-1; may need
   a small public `NetworkClient` predicate, so coordinate with the Network
   N0/N1 owner).
2. Render the owner/epoch/revision triple, or correct
   `docs/wiki/apps/network-settings.md:21` (P2-2).
3. Drop the unused `bssid` row key, or correct
   `docs/wiki/apps/network-settings.md:24` (P2-3).
4. Extend `check_boundary.cmake` to deny the text-entry control family and to
   assert a closed invokable set, so ADR-0055's accepted consequence holds
   (P2-4).

The six P3 items are non-blocking; folding P3-1, P3-2, P3-4, and P3-6 into the
same repair pass would be cheap. I will recheck the repaired exact commit
rather than approving prose. Acceptance will apply only to that exact SHA.
