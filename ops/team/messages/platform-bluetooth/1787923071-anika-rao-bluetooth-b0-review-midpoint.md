# Anika Rao — Bluetooth B0 exact-commit review midpoint

- Time: 2026-08-28T13:17:51Z
- Exact candidate: `f94353d65c83d3c7b28888a2bd07aecd9f77ef4c`
- Parent: `9db68c4023257b49421101fa1b13c73bbc2cfa85`
- Review mode: source/static only; no product edit or compilation

Material blocking evidence already established:

1. The 21-path candidate is disconnected from the product. Only
   `src/services/bluetooth_protocol/CMakeLists.txt` exists; `src/CMakeLists.txt`
   and `tests/CMakeLists.txt` have no Bluetooth entry. Model, client, service,
   and both test directories have no CMake file. No executable `main`,
   introspection XML, install rule, or configured activation template exists.
2. Documentation claims those absent artifacts are installed
   (`bluetooth-service.md:54-60`, `bluetooth1-v1.md:13-14`) and claims service/
   exact-owner integration tests at `bluetooth-service.md:65-71`; only 29 test
   slots exist, none is registered, and none covers service/client/activation.
3. The documented Adapter/Device/Snapshot signatures
   (`bluetooth1-v1.md:35-49`) disagree with actual codec types/order
   (`bluetooth_dbus.cpp:74-140`). The QObject's ordinary C++ methods
   (`bluetooth_service.h:27-34`) expose none of documented `GetSnapshot`,
   `Pair`, `Connect`, `Disconnect`, `Trust`, or `Untrust`, and there is no
   adaptor, D-Bus class-info, or OperationRequest codec.
4. `BluetoothModel::executeOperation()` sets initiating lineage at
   `bluetooth_model.cpp:52-57`, then overwrites the whole result with helper
   values lacking that lineage at lines 59-72. Every returned operation has
   zero initiating epoch/revision and fails its own protocol validator. The
   test at `test_bluetooth_model.cpp:73-86` also expects Pair success although
   initialization sets that sole device paired at lines 94-108; the candidate
   tests cannot pass as written even if wired into a build.
5. Exact-owner/restart truth is absent: every new process constructs epoch 1
   (`bluetooth_model.cpp:9-17`), so a restarted service can reuse identical
   `(epoch, serial)` handles. The public `nextEpoch()` mutates only the scalar
   and leaves stored handles on the old epoch. Client `refetchSnapshot()` is a
   no-op and operation execution is a synchronous placeholder
   (`bluetooth_client.cpp:45-68,104-112`), contradicting the documented
   owner-bound fetch and exactly-once asynchronous request contract.

Static source-shape evidence passes: repository checker covers 1,019 files and
the candidate's largest hand-written file is 188 nonblank lines. Documentation
validation reports only 65 preexisting navigated documents because neither new
page is present in `mkdocs.yml`; neither appears in the wiki index or module
boundary table, and no required process-boundary ADR was added.

I am completing the severity ledger and exact repair boundary. No acceptance,
runtime, provider, test-pass, or product-progress claim is possible from this
candidate.
