# Independent exact-candidate review — Bluetooth BlueZ B1 adapter

- Reviewer persona: **Betty Holberton** (`betty-holberton`), independent platform-adapter reviewer
- Provider/model: Moonshot Kimi `kimi-code/k3`, reasoning high
- Candidate SHA: `f44919a52f67515779f887b8d54a9bb2a57b3c4b` (verified `git rev-parse HEAD`)
- Tree SHA: `33376e05a4174024c0cd240b1408b183d2c10ed4` (verified `git rev-parse HEAD^{tree}`)
- Parent SHA: `f081a8686efc3df8d1bf0543eee5646be858945f` (preserved GLM WIP)
- Base SHA: `ce9228d9694622d503d92a38d01986f8f124f188` (full diff `ce9228d..f44919a` reviewed: 33 files, +4,328/−20)
- Review worktree: `/home/cabewse/work_SPaC3/container-wm-workers/bluetooth-bluez-b1-k3-review` (detached at candidate; `git status --porcelain` empty before and after all work; no product path edited)
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-bluez-k3`
- Implementer handoff: `ops/team/messages/platform-bluetooth/1788409993-thelma-estrin-handoff.md`

## Review questions (evidence)

### 1. Authority split (ADR-0037) — PASS

`rg` over `src/services/bluetooth_bluez_adapter` enumerates every D-Bus method call the adapter makes
(`src/services/bluetooth_bluez_adapter/src/bluez_transport.cpp`):

- `org.freedesktop.DBus.GetNameOwner` (owner resolution, bluez_transport.cpp:202-206)
- `org.freedesktop.DBus.ObjectManager.GetManagedObjects` (bluez_transport.cpp:157-159)
- `org.freedesktop.DBus.Properties.Set(Powered)` (bluez_transport.cpp:356-364)
- `org.bluez.Adapter1.StartDiscovery` / `StopDiscovery` (bluez_transport.cpp:367-377)
- `org.bluez.Device1.Connect` / `Disconnect` (bluez_transport.cpp:379-389)

No `Pair`, `Trust`/`Untrust`, `RemoveDevice`, `SetPairable`, `CancelPairing`, `SetDiscoveryFilter`, or
agent registration call exists anywhere in the module. The boundary checker
(`tests/services/bluetooth_bluez_adapter/check_boundary.cmake:31-36`) enforces exactly this method set,
and `check_boundary_negative.cmake` proves it rejects a `QStringLiteral("Pair")` poison. Paired/Connected
are read-only projections (`bluez_object_store.cpp`); BlueZ remains the authority, matching ADR-0037 and
ADR-0056's unchanged authority split.

### 2. Injected bus, no host contact — PASS

- `BluezAdapterBackend` takes an injected `QDBusConnection` (public header, bluez_adapter_backend.h:30-32);
  the adapter module never calls `QDBusConnection::systemBus()`/`sessionBus()` (boundary checker
  check_boundary.cmake:53-64 enforces this for test sources; product sources take the connection by value).
- Tests run a fake `org.bluez` `QDBusVirtualObject` on a private `dbus-daemon` per row
  (`support/private_bus.h`, `support/fake_bluez.{h,cpp}`).
- Proof run: the full focused selector passed with the host system bus poisoned:
  `DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest ...` → 14/14 Debug, 14/14 Release (exact commands below).

### 3. Lineage and failure — PASS

- Owner loss/replacement: `BluezTransport` resolves the exact unique owner, addresses every call to it, and
  advances `m_ownerToken` on any owner transition (`bluez_transport.cpp:234-248`); every pending reply is
  fenced by the captured token (`bluez_transport.cpp:163-176`, `339-352`). `handleOwnerReplaced`
  (`bluez_adapter_publication.cpp:31-55`) completes outstanding/queued operations `Uncertain/authority-replaced`,
  drops all leases, clears the store, and republishes — proven by `ownerLossAndReturnRetiresTruth` and
  `ownerLossDuringDeferredConnectIsUncertain` (including the late-reply no-second-delivery check).
- Generation fencing: `start()` returns the generation before the run may publish; `submit()` captures the
  generation on a queued invocation and drops superseded work (`bluez_adapter_backend.cpp:105-158`); proven
  by `eventsAfterStopDoNotPublish`.
- Discovery lease refcount: one `StartDiscovery` per adapter regardless of caller count, `StopDiscovery`
  exactly at the last release; proven by `discoveryLeaseAcquireReleaseRefcount` (startDiscoveryCalls==1,
  stopDiscoveryCalls==1), `ownerVanishedReleasesLeases`, `leaseDiesWithAdapterPowerOff` (no resurrection),
  `backendEnforcesLeaseCaps` (16 succeeded + 1 `too-many-leases`, matching kMaxDiscoveryLeasesPerAdapter=16,
  kMaxDiscoveryLeasesTotal=64 in bluetooth_limits.h:21-22), and `externalDiscoverySessionReconciled`
  (synthetic external row stays truthful).
- Error mapping: org.bluez errors map to typed results (`mapErrorReply`, bluez_adapter_backend.cpp:25-47);
  proven by `connectSuccessAndFailureReplies` / `disconnectSuccessAndFailureReplies`
  (`already-connected`, `not-connected`, `bluez-error`, `stale-handle`, `adapter-off`).
- Hostile bounds: canonical address grammar, UTF-8-safe name truncation without splitting sequences,
  control-character sanitization, RSSI only in [-128,0], 24-bit CoD decoding, unknown interfaces ignored,
  8192-object parse cap, Bluetooth1 inventory caps — proven by `hostilePropertiesAreBounded`,
  `duplicateAdapterAddressDeduplicated`, `duplicateDeviceAddressDeduplicated`, and by my own scratch attack
  below (object flood → whole-drop fail-closed).

**Reviewer attack (own fake sequences, scratch code in the build root only):** compiled a scratch probe
(`/home/cabewse/work_SPaC3/builds/qindaqt/review-bluez-k3/attack/tst_bluez_attack.cpp`) reusing the
shipped fake on a private bus, driving a managed-object flood of 8,193 adapters (past the
`kMaxParsedObjects` = 8,192 cap in bluez_transport.cpp:28): observed `Unavailable/no-adapter` with an
empty inventory — the flood is dropped whole, never partially published (3/3 scratch assertions pass, exit 0).

### 4. Composition root — PASS (with the one declared caveat reproduced)

- `src/services/bluetooth_service/app/main.cpp` selects the backend only through
  `resolveBluetoothBackendMode(QINDAQT_BLUETOOTH_BACKEND)`; only the exact value `deterministic` picks the
  B0 empty backend; everything else fails closed to the production `BluezAdapterBackend(QDBusConnection::systemBus())`
  (`bluez_backend_mode.cpp`, proven by `tst_bluez_backend_mode.cpp` rows incl. casing/whitespace/banana).
- Activation descriptor and systemd unit are untouched by the diff; `qindaqt.bluetooth-activation` passes
  in both profiles.
- The new B1-owned installed-package row `qindaqt.bluetooth-bluez-installed-boundary` stages only
  `QindaQtBluetoothB1` and verifies the exact archive + two-header public surface (passed in both profiles).
- The pre-existing whole-repository row `qindaqt.bluetooth-staged-install`
  (tests/services/bluetooth_service/CMakeLists.txt:64) exists and was run by me in Debug: it fails
  (`exit 8`) at the first unrelated unbuilt artifact
  `src/profiles/libqindaqt_profiles.a` (`file INSTALL cannot find ... libqindaqt_profiles.a`), exactly as
  the handoff reported. It performs an unscoped whole-repository install, so passing it requires building
  the entire repository — prohibited by this review lane's focused-build restriction. This is stated, not
  inferred; the B1-owned row above covers the adapter's own packaged surface.

### 5. Boundaries and docs — PASS

- `git diff --name-status ce9228d..f44919a` shows zero changes under `bluetooth_protocol`,
  `bluetooth_model`, `bluetooth_client`, applets, or any applet protocol path — those modules are
  byte-identical to base.
- Shared-registry edits are purely additive: one `add_subdirectory` line each in `src/CMakeLists.txt` and
  `tests/CMakeLists.txt`, one mkdocs nav line, one ADR index row, one module-boundaries row.
- The adapter links only `QindaQt::BluetoothModel` + Qt Core/DBus (adapter CMakeLists.txt:26-30); public
  headers carry no transport internals (checker-enforced, installed-boundary row).
- AGENT markers present and accurate (AGENT-CONTRACT/GUARD/NOTE throughout transport, backend, publication,
  store, mode).
- `./tools/check-source-shape`: exit 0, 1,780 files; largest owned file 499 nonblank lines
  (`bluez_adapter_backend.cpp`); the two warnings are pre-existing non-owned files.
- ADR-0056 and the bluetooth-service wiki claim only fake-qualified behavior (no physical radio, no
  pairing UX, no audio routing); the qualification section states exactly the fake's limits.
- `./tools/validate-docs` (117 documents) and `mkdocs build --strict` both exit 0.
- One P3 doc precision item below.

## Findings ledger

### P0 — none

### P1 — none

### P2 — none

### P3

1. **Doc overstatement in the observed-property list.** `docs/wiki/architecture/bluetooth-service.md`
   (Production BlueZ adapter section) states the adapter "observes Address, Alias/Name, Powered,
   Discovering, Adapter, Class, Icon, RSSI, Paired, Connected, and Trusted". The store never reads
   `Trusted`: `BluezDeviceState` (bluez_object_store.h:31-42) has no trusted field and `upsertDevice`
   (bluez_object_store.cpp:305-370) never touches it, so a Trusted property change is silently dropped
   rather than observed. Harmless (Trusted is correctly never published nor mutated, matching the authority
   split), but the sentence lists one property the code does not observe. Nonblocking precision; a one-word
   doc edit fixes it.
2. **Two fail-closed paths lack a shipped row.** No shipped row exercises a *failed* `GetManagedObjects`
   reply (whole-drop to empty, bluez_transport.cpp:169-175) or a `PropertiesChanged` carrying invalidated
   properties (re-enumeration path, bluez_adapter_publication.cpp:86-90); the shipped fake cannot emit
   either. Both paths are simple fail-closed code verified by reading, and the wiki's qualification list
   does not claim them as covered rows. Nonblocking coverage precision; the shipped fake could grow a
   deferred/error GetManagedObjects mode later.

## Exact commands and results

All from the review worktree unless noted; build root `<ROOT>` = `/home/cabewse/work_SPaC3/builds/qindaqt/review-bluez-k3`.

| Command | Result |
| --- | --- |
| `git rev-parse HEAD` / `HEAD^{tree}` / `HEAD^`; `git status --porcelain` (before and after) | candidate/tree/parent as above; porcelain empty both times |
| `cmake -S . -B <ROOT>/debug -G Ninja -C .../qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON` | exit 0 |
| Same configure under `<ROOT>/release` with `-DCMAKE_BUILD_TYPE=Release` | exit 0 |
| `cmake --build <ROOT>/debug --parallel 3 --target qindaqt_bluez_adapter_tests qindaqt_bluez_adapter_operations_tests qindaqt_bluez_backend_mode_tests qindaqt_bluetooth_protocol_tests qindaqt_bluetooth_model_tests qindaqt_bluetooth_deterministic_backend_tests qindaqt_bluetooth_client_tests qindaqt_bluetooth_qt_transport_tests qindaqt_bluetooth_activation_tests qindaqt_bluetooth_service_tests qindaqt_bluetooth_lease_owner_loss_tests qindaqt-bluetooth-service` | exit 0, 91/91 actions |
| Same focused targets under `<ROOT>/release` | exit 0, 91/91 actions |
| `DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir <ROOT>/debug -R '^qindaqt\.bluetooth-' -E '^qindaqt\.bluetooth-staged-install$' --output-on-failure --no-tests=error` | exit 0, **14/14 passed** (6 `bluetooth-bluez-*` rows included) |
| Same selector under `<ROOT>/release` | exit 0, **14/14 passed** |
| `ctest --test-dir <ROOT>/debug -R '^qindaqt\.bluetooth-staged-install$' --output-on-failure --no-tests=error` | row fails as expected: `file INSTALL cannot find <ROOT>/debug/src/profiles/libqindaqt_profiles.a` — whole-repo install cannot run under the focused-build restriction (see Q4) |
| `./tools/validate-docs` | exit 0, 117 Markdown documents + navigation validated |
| `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir <ROOT>/site` | exit 0 |
| `./tools/check-source-shape` | exit 0, 1,780 files; largest owned production file 499 nonblank lines; 2 warnings on pre-existing non-owned files |
| `git diff --check` (worktree at candidate) | exit 0 |
| `python3 -m json.tool` | not applicable — `git diff --name-only ce9228d..f44919a` contains 0 `.json` files |
| Scratch reviewer attack (own fake sequences): compile+run `<ROOT>/attack/tst_bluez_attack` (8,193-adapter managed-object flood past `kMaxParsedObjects`) | exit 0, 3/3 assertions: flood dropped whole → `Unavailable/no-adapter`, empty inventory, never partial |

Not run: `tests/session` nested-compositor rows, host D-Bus services, hardware, uinput, network — per lane rules.

## Verdict

The candidate implements the `AdapterBackend` port with exact-owner fencing, generation fencing, the
bounded caller-scoped discovery lease table, typed error mapping, and hostile-input bounds, all exercised
by substantive private-bus rows plus my own flood attack; pairing/trust/records authority stays in BlueZ;
tests and product code never contact a host bus; the composition root fails closed to production. Two P3
doc/coverage precision items are nonblocking.

**VERDICT ACCEPT P0/P1/P2/P3=0/0/0/2**
