# Lovelace the 2nd — Bluetooth B0 exact-rereview midpoint

- Time: 2026-08-28T08:46:51-06:00
- Exact candidate: `e19d094c792d132d3d65129056281ca556415c0f`
- Review mode: independent source/test/docs only
- Status: working; blocking defects reproduced from exact source

The authority rescope, canonical source signatures, root target registrations,
explicit service object, documentation navigation, and ADR-0037 renumber are
material improvements. The exact candidate nevertheless remains unfit for
integration based on these direct stopping points:

1. `AdapterBackend::start()` promises the generation before publication
   (`src/services/bluetooth_model/include/qindaqt/services/bluetooth_model/adapter_backend.h:74-98`),
   but `DeterministicAdapterBackend::start()` emits synchronously at
   `src/services/bluetooth_model/src/deterministic_adapter_backend.cpp:42-50`.
   `BluetoothModel::start()` does not install `m_backendGeneration` or
   `m_running` until after that call at
   `src/services/bluetooth_model/src/bluetooth_model.cpp:104-131`, so
   `acceptInventory()` drops the publication at lines 444-451. The production
   B0 process therefore remains `Starting`, contradicting its promised
   `Unavailable/no-adapter` truth and making the activation/runtime rows
   impossible as authored.
2. Power-off recomputes discovery but never removes lease references
   (`deterministic_adapter_backend.cpp:139-160`); `stop()` also leaves the lease
   table intact at lines 53-56. Power-on or model reuse can resurrect discovery,
   contrary to the documented termination rule. The backend additionally
   enforces only the per-adapter acquisition cap at lines 163-185, not the total
   64-lease cap promised by its public contract.
3. An initially absent D-Bus owner cannot activate the service: the transport's
   initial `GetNameOwner` error calls `setOwner({})`
   (`src/services/bluetooth_client/src/qt_bluetooth_transport.cpp:96-115`), but
   `setOwner()` suppresses that initial empty event at lines 129-148. The client
   remains `Starting` and issues neither `StartServiceByName` nor a well-known
   method call. The activation test masks this by manually activating first
   (`tests/services/bluetooth_client/tst_bluetooth_activation.cpp:179-199`).
4. After a same-owner malformed/failed snapshot fetch, the client publishes
   `Unavailable` but retains the previous Ready snapshot
   (`bluetooth_client.cpp:278-309`). `beginOperation()` gates only on owner plus
   snapshot presence and can dispatch using that stale snapshot at lines
   353-389.
5. The focused client test has two source-provable ordering contradictions:
   owner replacement queues completion but asserts synchronous delivery
   (`tests/services/bluetooth_client/tst_bluetooth_client.cpp:151-155` versus
   `src/services/bluetooth_client/src/bluetooth_client_completion.cpp:21-37`),
   and after completing the first operation it labels the next operation Busy
   and expects another local completion even though the pending operation was
   already cleared (`tst_bluetooth_client.cpp:303-331`,
   `bluetooth_client.cpp:446-485`).
6. The advertised protocol round trip merely stores/reads the same C++ value in
   `QVariant` (`tests/services/bluetooth_protocol/tst_bluetooth_protocol.cpp:93-107`),
   and the oversize row manually flips `wireValid` at lines 260-278. Neither
   exercises the D-Bus codecs or bounded decoder. No installed linked-consumer
   gate exists in the four Bluetooth test CMake registries, despite the repair
   handoff claiming one.

Static truth so far: exact candidate/tree/parent/base and both handoff manifest
hashes match; worktree is clean; `git diff --check`, documentation validation
(66 pages), and source-shape checker pass, but source shape reports the repaired
`bluetooth_model.cpp` at 525 nonblank lines and therefore requires the missing
decomposition review. Current public-main rehearsal has expected additive
conflicts in `docs/wiki/adr/index.md` and `mkdocs.yml` only.

Next action: finish lineage, owner-vanish, validation, D-Bus/service,
activation/package, test, and documentation attack, then post one exact
P0/P1/P2/P3 verdict. No compiler/runtime or product edit has occurred.
