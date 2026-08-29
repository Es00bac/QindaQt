# 2026-08-28T18:00:44Z — Cassia Rowan — Bluetooth B0 Resume Update

- Author: Cassia Rowan (Google Antigravity Vertex ADC `gemini-3.7-flash-high`, reasoning: high)
- Role: permanent Bluetooth B0 rescue/finish partner
- Base: `e19d094c792d132d3d65129056281ca556415c0f`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/bluetooth-b0`
- Branch: `worker/bluetooth-b0`

## Staged and Unstaged Ownership

1. **Inherited Staged Repair (Preserved)**:
   - 33 paths, +1586/-404 from Samira Cole's candidate, addressing Lovelace the 2nd's 0/9/5/3 FAIL ledger (ADR-0037 alignment, wire decoders, bounds, preflights, fake backends).
2. **Current Unstaged Fixes**:
   - `DeterministicAdapterBackend::stop()` clears leases and state leases.
   - `BluetoothModel::validateRequest` checks caller safety first, then target validity before availability.
   - `BluetoothModel::safeCallerId` enforces D-Bus unique connection name formatting (`:1.x`).
   - `BluetoothClient::acceptInvalidation` refetches on equal or greater revisions.
   - `QtBluetoothTransport::queryInitialOwner` activates service on `NameHasNoOwner` / `ServiceUnknown`.
   - `QtBluetoothTransportTests` fixes moved pointer dereference.
   - `tst_bluetooth_service.cpp` and `tst_bluetooth_lease_owner_loss.cpp` fix D-Bus well-known bus name format (`.p%1` prefix).

## Next Gate

Applying remaining targeted code edits, executing full `ctest --output-on-failure` suite in Debug and Release, verifying packaging/installed-consumer gates, formatting, docs/MkDocs checks, and preparing the single descendant commit crediting Samira.
