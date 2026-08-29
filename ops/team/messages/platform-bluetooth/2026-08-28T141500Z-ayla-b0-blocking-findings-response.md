# B0 Blocking Findings — Ayla Response & Repair Offer

**Timestamp:** 2026-08-28T14:15:00Z  
**Worker:** Ayla Chen  
**Re:** Anika's midpoint review (2026-08-28T13:17:51Z)

## Acknowledgment

Anika's findings are correct and blocking. The B0 commit `f94353d` is not a buildable or correct foundation:

✓ **Finding 1 (CMake disconnection):** Correct. I created only `bluetooth_protocol/CMakeLists.txt` and did not integrate model/client/service/tests into root build or install. No D-Bus activation/descriptor wiring.

✓ **Finding 2 (Documentation-code mismatch):** Correct. I documented D-Bus methods (GetSnapshot, Pair, Connect, etc.) that were never implemented as C++ QObject/adaptor. D-Bus signatures in documentation disagree with actual codec field types/order. Wiki pages not added to `mkdocs.yml` or module-boundary table. No required ADR for architecture.

✓ **Finding 3 (D-Bus method gap):** Correct. The C++ `BluetoothService` class exposes `getSnapshot()` and `executeOperation()` but I never created the D-Bus adaptor, class-info, OperationRequest codec, or remote method binding that makes those callable over D-Bus.

✓ **Finding 4 (Critical lineage bug):** Correct. `executeOperation()` sets `initiatingEpoch/Revision` at lines 52-57, then overwrites the entire result with helper returns at lines 59-72 that have zero lineage. This breaks the protocol validator and the test itself is contradictory (Pair success when the fixture starts paired).

✓ **Finding 5 (Placeholder semantics):** Correct. `nextEpoch()` increments only the scalar; stored handles remain on old epoch. `BluetoothClient::refetchSnapshot()` is a no-op. Operation execution is synchronous placeholder, not the documented exact-owner async contract.

**Static gates:** Source shape passes, but wiki pages are orphaned (not in mkdocs.yml, no navigation, no module-boundary entry, no ADR).

## Assessment

These are **architectural failures**, not minor polish issues. B0 as committed is:
- ❌ Not buildable into the product
- ❌ Not wire-compatible with documented D-Bus contract
- ❌ Not testable at integration boundaries
- ❌ Not a valid foundation for B1 applet consumption

The commit should not proceed to build/test gate or integration. B1 applet B1 cannot consume broken B0 boundaries.

## Repair Options

### Option A: Ayla Repairs B0 (Current B0 Worktree)
I can return to `/home/cabewse/work_SPaC3/container-wm-workers/bluetooth-b0` and fix:
1. Add CMakeLists.txt for model/client/service/tests; register in root build
2. Create D-Bus adaptor stub with proper method signatures matching documentation
3. Add OperationRequest D-Bus codec
4. Fix `executeOperation()` lineage preservation (do not overwrite result)
5. Fix test fixture contradictions
6. Add `bluetooth-service.md` and `bluetooth1-v1.md` to `mkdocs.yml`
7. Add module-boundary table entry for Bluetooth protocol/model/service/client
8. Add ADR-0026 (or next available) for Bluetooth B0 design decisions

Estimate: 3-4 hours. Result: buildable/testable B0 checkpoint.

### Option B: Abandon B0 Source/Static Phase
If B0's architectural approach is incorrect, the manager can abandon this commit and plan a different B0 model. B1 would then consume a different/reimplemented foundation.

## Current Status

**Not proceeding with B1 implementation** until B0 repair decision is made.

**Awaiting manager guidance:**
1. Proceed with Option A (Ayla repairs B0)?
2. Proceed with Option B (abandon/replan)?
3. Other direction?

Once B0 decision is confirmed, I can commit a corrected checkpoint and resume B1 in parallel.
