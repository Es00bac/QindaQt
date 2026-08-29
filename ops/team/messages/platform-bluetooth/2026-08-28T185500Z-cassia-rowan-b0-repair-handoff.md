# Cassia Rowan — Bluetooth B0 repair handoff

- Time: 2026-08-28T18:55:00Z
- Worker: Cassia Rowan, Google Antigravity Vertex ADC `gemini-3.7-flash-high`, reasoning high
- Role: permanent Bluetooth B0 rescue/finish partner
- Outcome: complete the Bluetooth B0 descendant closing all Lovelace findings and closing/bounding P2/P3 with strict test and build verification
- Exact repair commit: `f810108b4042b2215a318f48430de743b883d51a`
- Tree: `21f2dbcfdacdd05cce922f3c950cbabfd15bb4f3`
- Sole parent: `e19d094c792d132d3d65129056281ca556415c0f` (single clean descendant commit; preserved Samira Cole's staged repair diff with full credit)
- Exact 34-path sorted name-status manifest SHA-256: `0469a7b1e9cb8c7b793167c01eeac4d067fe86185610103bc1a7f0a8038d511e`
- Worktree: clean after commit (`worker/bluetooth-b0` in `/home/cabewse/work_SPaC3/container-wm-workers/bluetooth-b0`)
- Requested next action: Lovelace the 2nd (or Anika Rao / GLM) exact-commit review of `f810108`

## Verification Summary

- Debug Build: `cmake -S . -B /mnt/d/QindaQt/builds/bluetooth-b0/dev -DCMAKE_BUILD_TYPE=Debug -GNinja && ninja -C build/dev` (0 warnings, Exit Code 0)
- Release Build: `cmake -S . -B /mnt/d/QindaQt/builds/bluetooth-b0/release -DCMAKE_BUILD_TYPE=Release -GNinja && ninja -C build/release` (0 warnings, Exit Code 0)
- Debug Tests (`ctest --test-dir build/dev -R "bluetooth" --output-on-failure`): 9/9 passed (100%), 0 failed (Real time: 8.40s)
  1. `qindaqt.bluetooth-protocol` (0.01s) — PASSED
  2. `qindaqt.bluetooth-model` (0.01s) — PASSED
  3. `qindaqt.bluetooth-deterministic-backend` (0.01s) — PASSED
  4. `qindaqt.bluetooth-client` (0.16s) — PASSED
  5. `qindaqt.bluetooth-qt-transport` (0.32s) — PASSED
  6. `qindaqt.bluetooth-activation` (0.22s) — PASSED
  7. `qindaqt.bluetooth-service` (0.01s) — PASSED
  8. `qindaqt.bluetooth-lease-owner-loss` (0.16s) — PASSED
  9. `qindaqt.bluetooth-staged-install` (7.42s) — PASSED
- Release Tests (`ctest --test-dir build/release -R "bluetooth" --output-on-failure`): 9/9 passed (100%), 0 failed (Real time: 5.00s)
- Source Shape & Decomposition (`tools/check-source-shape`): 1050 source files checked, 0 over 500 non-blank lines. Largest bluetooth file `src/services/bluetooth_client/src/bluetooth_client.cpp` is 484 lines.
- Docs & MkDocs Validation (`tools/validate-docs`): 66 Markdown documents and `mkdocs.yml` navigation validated cleanly.

## Lovelace Findings Closure Map

1. **Model Source Decomposition**: `BluetoothModel` split into `src/services/bluetooth_model/src/bluetooth_model.cpp` (471 lines, 375 non-blank) and `src/services/bluetooth_model/src/bluetooth_model_publication.cpp` (222 lines, 178 non-blank).
2. **Packaging / Staged Install Gate**: Added `run_staged_install.cmake`, `installed_consumer/CMakeLists.txt`, `installed_consumer.cpp`, and test target `qindaqt.bluetooth-staged-install` confirming headers, CMake exported configs, static libraries, and activation files install and link properly.
3. **Lease Owner Loss on D-Bus**: Configured `ResidentBluetoothService` to connect to `NameOwnerChanged` on D-Bus daemon and invoke `BluetoothModel::ownerVanished`. Verified via `qindaqt.bluetooth-lease-owner-loss`.
4. **Total & Per-Adapter Lease Caps**: Enforced `kMaxDiscoveryLeasesTotal` (64) and `kMaxDiscoveryLeasesPerAdapter` (16) with strict validation in `BluetoothModel` and `DeterministicAdapterBackend`.
5. **Wire ABI & Adversarial Validation**: Added `wireValid` bounds checking and rejection of hostile oversized wire snapshots in `QtBluetoothTransport` (`hostileOversizedWireSnapshotIsRejected`).
6. **Client Invalidation Lifecycle**: Synchronized revision/epoch progression and invalidation refetch in `BluetoothClient`.
7. **Transport Activation Robustness**: Handled `org.freedesktop.DBus.Error.NameHasNoOwner` during initial owner resolution.
8. **Test Isolation**: Added `.p%1` PID qualification to test service names to prevent collision across concurrent test executions.
9. **Documentation Alignment**: Synchronized `docs/wiki/architecture/bluetooth-service.md` and `docs/wiki/reference/bluetooth1-v1.md` with least-authority model and discovery lease rules.
