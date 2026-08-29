# Anika Rao — Bluetooth B0 exact-commit review FAIL

- Time: 2026-08-28T13:19:46Z
- Reviewer: Anika Rao, immutable internal AppShell engineer persona
- Exact candidate: `f94353d65c83d3c7b28888a2bd07aecd9f77ef4c`
- Tree: `20a9e834b5441a421564e6154f2b9d24b26599d0`
- Sole parent and merge base: `9db68c4023257b49421101fa1b13c73bbc2cfa85`
- Exact 21-path sorted name-status manifest SHA-256:
  `f067801698232dcecd700657dbb16d0e8702d4ba5aabfb8c22cfb554f456683b`
- Worktree: clean; no product edit
- Review mode: source/static only; no configure, compile, test binary, D-Bus,
  BlueZ, rfkill, hardware, UI, or host-state action
- Verdict: **FAIL for integration and for QQ-005.05 evidence advancement**
- Findings: **P0/P1/P2/P3 = 0/8/4/3**

This commit must be preserved, but it is neither a buildable Bluetooth
foundation nor a faithful implementation of the accepted Bluetooth1 plan.
Its source shape is small; the blockers are product-boundary, protocol,
lineage, lifecycle, build, and evidence defects rather than file size.

## P0

None. The candidate has never been run and contains no demonstrated host
mutation or destructive behavior.

## P1 — blocking

### P1-1: The candidate implements the wrong Bluetooth authority boundary

The accepted platform-service plan says paired-device control first: adapter
power, inventory, a reference-counted discovery lease, and connect/disconnect
of already-paired devices. BlueZ owns pairing, trust, keys, device records,
profiles, and authorization; later pairing belongs in a separate Agent1
outcome with explicit prompts/deadlines
(`platform-services/1787853847-samira-cole-plan-handoff.md:263-294`).

The candidate instead makes `Pair`, `Trust`, and `Untrust` public operation
kinds (`bluetooth_types.h:41-47`), documents them as B0 methods
(`bluetooth1-v1.md:56-68`), and directly mutates locally duplicated `paired`
and `trusted` flags (`bluetooth_model.cpp:113-137,191-216`). It has no adapter
power operation and no owner-bound discovery lease. `beginDiscovery()` is an
unexposed boolean fake method, not a reference-counted lease
(`fake_adapter_interface.h:27-35`). This reverses the accepted authority split
and risks duplicating BlueZ truth. Repair requires re-scoping B0, not merely
renaming methods.

### P1-2: None of the Bluetooth outcome is connected to the product build

The candidate adds only one isolated build file,
`src/services/bluetooth_protocol/CMakeLists.txt:1-24`. `src/CMakeLists.txt:29-48`
and `tests/CMakeLists.txt:36-55` contain no Bluetooth registry entry. Model,
client, service, and both test directories have no `CMakeLists.txt`; there is
no executable `main`, target/install/export/package definition, configured
activation template, or service/client linked-consumer gate. The claimed
introspection XML is absent from the exact 21-path manifest.

Consequently no Bluetooth source or test is configured by the repository and
the two activation files retain literal `@...@` placeholders. This is not a
buildable or installable source slice.

### P1-3: Deterministic source-level compile blockers exist even before wiring

- `bluetooth_service.h:8` includes nonexistent `<QtCore/memory>`; the exact
  installed Qt 6.11.1 header root has `QtCore/QObject` but no `QtCore/memory`.
- `bluetooth_model.h:40` uses `std::unique_ptr` without including `<memory>`;
  `bluetooth_model.cpp:14` uses `std::make_unique` and lines 118/144/170/196
  use `std::find_if` without owning `<memory>`/`<algorithm>` includes.
- `bluetooth_client.h:60-61` repeats `Q_DECLARE_METATYPE` for `Snapshot` and
  `OperationResult` already declared by its included
  `bluetooth_types.h:145,147`, producing duplicate specialization definitions.
- `bluetooth_client.cpp:97-100` uses `QDBusReply` and dereferences the
  connection interface without owning their public includes.

No compiler was invoked by this review; these are direct include/declaration
defects visible in the immutable source.

### P1-4: The documented Bluetooth1 wire ABI does not match the codec or service

The reference declares Adapter `((tt)ssunnb)`, Device
`((tt)(tt)ssnnbbbbbuu)`, and Snapshot
`(uttssbba((tt)ssunnb)a((tt)(tt)ssnnbbbbbuu))`
(`bluetooth1-v1.md:31-49`). The actual writers encode Adapter as
`((tt)ssubu)`, Device as `((tt)(tt)ssunbbbu)`, and Snapshot as those two arrays
after `(uttss)` (`bluetooth_dbus.cpp:74-140`). Adapter field order also differs:
the code sends state, discovering, capabilities while the table says state,
capabilities, discovering.

The service class has only ordinary C++ `getSnapshot()` and
`executeOperation(OperationRequest)` methods (`bluetooth_service.h:27-34`).
They are neither slots nor invokable methods; there is no adaptor, interface
class-info, XML, or separate documented `GetSnapshot`/`Pair`/`Connect`/
`Disconnect`/`Trust`/`Untrust` surface. `OperationRequest` has no D-Bus codec
and is not registered in `registerDBusTypes()` (`bluetooth_dbus.cpp:44-56`).
`ExportAllContents` therefore cannot manufacture the documented interface.

The fixed v1 schema cannot be accepted until one canonical XML/codec/API and
signature-verification test agree byte-for-byte.

### P1-5: Every model operation result loses its initiating lineage

`BluetoothModel::executeOperation()` sets initiating epoch/revision on one
local result (`bluetooth_model.cpp:50-57`), then overwrites the complete object
with a fresh helper result at lines 59-72. Each helper initializes neither
initiating field (`bluetooth_model.cpp:113-216`). All returned operation
results therefore carry zero initiating epoch/revision and fail
`validateOperationResult()` at `bluetooth_validation.cpp:140-145`.

The model's own tests expose a second deterministic contradiction: initialization
sets the only device `paired = true` (`bluetooth_model.cpp:94-108`), while
`testPairOperation()` expects pairing that same handle to succeed
(`test_bluetooth_model.cpp:73-86`). Production code correctly returns
`already-paired` at lines 126-129, so this test cannot pass. No executable test
was run; source logic alone proves both contradictions.

### P1-6: Restart lineage and the public client are placeholders, not exact-owner behavior

Every new model starts epoch 1 with the same serial sequence
(`bluetooth_model.cpp:9-17,80-110`). A restarted service can therefore issue
identical `(epoch, serial)` handles, directly contradicting restart invalidation
(`bluetooth-service.md:26-34`, `bluetooth1-v1.md:92-106`). Public
`nextEpoch()` changes only `m_epoch` (`bluetooth_model.cpp:21-24`) and leaves
all stored handles at the old epoch, making the next snapshot invalid.

The client never fetches a snapshot (`bluetooth_client.cpp:104-112`), yet
`bindService()` marks itself connected and emits success
(`bluetooth_client.cpp:28-38`). It never connects to the exact owner's Changed
signal; `onSnapshotChanged()` has no connection. Operations synchronously
return a fabricated timeout (`bluetooth_client.cpp:45-68`) rather than the
documented nonzero request ID and queued exactly-once completion
(`bluetooth1-v1.md:98-106`). Owner loss clears the owner but retains the stale
snapshot (`bluetooth_client.cpp:76-92`). Initial owner acquisition is ignored
by the change handler unless its old owner already matches local state.

This cannot support safe stale-reply rejection, uncertainty, invalidation
coalescing, or owner replacement.

### P1-7: Validation does not implement the documented fail-closed contract

The reference requires canonical Bluetooth addresses, ascending serials,
unknown RSSI exactly zero, and rejection of unknown enum/capability bits and
malformed state (`bluetooth1-v1.md:51-54,114-120`).
`validateSnapshot()` checks only state-enum membership, handle epoch,
duplicates, text length, adapter membership, and RSSI range when known
(`bluetooth_validation.cpp:81-126`). It accepts:

- arbitrary non-MAC address strings;
- unknown adapter/device capability bits;
- out-of-order arrays;
- nonzero RSSI when `rssiKnown == false` (`validRssi()` returns true
  unconditionally at lines 30-36);
- trusted or connected unpaired devices, connected devices on powered-off
  adapters, and discovering powered-off adapters;
- unstructured/control-bearing reason codes.

The service does not validate inbound requests before mutation, and the writer
does not bound arrays (`bluetooth_dbus.cpp:14-22`) despite the statement that
publishers reject oversized values atomically (`bluetooth1-v1.md:86-90`).
These are compatibility and fail-closed violations in the foundational wire
boundary.

### P1-8: Test and qualification claims are materially false

The handoff claims 38+ tests; the exact source has 28 named test slots plus one
`initTestCase`, and none is registered in CMake. Protocol coverage contains
only a Handle D-Bus round trip (`test_bluetooth_types.cpp:169-181`), not the
claimed exact Adapter/Device/Snapshot/OperationResult signatures, bounded-array
decode, aggregate limits, ordering, capability rejection, or malformed wire
matrix. Model coverage contains the contradictions above, no epoch rollover
snapshot validation, no capability/adapter-state matrix, and no emitted-change
assertions. There are zero service, client, activation, owner-replacement,
timeout, teardown, or private-bus tests.

Nevertheless `bluetooth-service.md:65-71` says focused tests cover exact
signatures/aggregate limits and integration tests verify activation,
publication, atomicity, and exact-owner replacement. That evidence does not
exist and must be removed until executable gates are present.

## P2 — serious repair findings

### P2-1: Public ownership, lifetime, and threading contracts are absent and unsafe

`BluetoothClient` stores the caller's `QDBusConnection &` indefinitely
(`bluetooth_client.h:24,52`) without a lifetime contract. A cheap connection
handle should normally be retained by value, or the caller-owned lifetime must
be explicit and enforced. `BluetoothModel::adapters()`/`devices()` return
references to mutable internal lists (`bluetooth_model.h:25-29`) with no
retention/invalidation rule. Public `nextEpoch`/`nextRevision`/`nextSerial`
mutators expose lineage authority and can break invariants. No public header
states GUI/main-thread affinity, queued completion behavior, destruction
cancellation, or error ownership as required by the repository API policy.

### P2-2: The claimed private fake backend is public, unused, and partly a no-op

The fake is under the public include tree despite an `AGENT-GUARD` saying it is
private (`fake_adapter_interface.h:10-14`). `BluetoothModel` constructs it but
never reads or calls it after construction; model state is duplicated in
separate lists (`bluetooth_model.cpp:9-16,80-110`).
`addDiscoveredDevice()` ignores address, name, and RSSI and returns success
without adding anything (`fake_adapter_interface.cpp:48-57`). This contradicts
the claim that the private fake owns state transitions
(`bluetooth-service.md:14-16`) and prevents meaningful backend-port tests.

### P2-3: Service connection ownership and export policy are inconsistent

`activateService()` registers on its injected connection
(`bluetooth_service.cpp:22-40`) but retains no connection identity.
`deactivateService()` always uses the ambient session bus at lines 43-48, so a
private-bus activation would be torn down on the wrong connection. Broad
`ExportAllContents` at lines 34-35 also exports future QObject additions by
default instead of an explicit versioned adaptor. Model mutations never emit
`snapshotChanged`, and service construction never connects that signal to
`changed`; even a corrected D-Bus surface would publish no invalidation.

### P2-4: Operation policy ignores capabilities and several required states

Pair/connect/disconnect/trust handlers search only the device list and mutate
local flags (`bluetooth_model.cpp:113-216`). They never confirm the parent
adapter exists/is powered, the advertised capability permits the operation, or
connected/already-connected and trust/pair consistency. The so-called
unpaired-connect test actually uses a nonexistent handle
(`test_bluetooth_model.cpp:137-147`), leaving the `not-paired` branch untested.
No operation uses `m_fakeAdapter`, no mutation emits an invalidation, and
`OperationRequest::secondary` has no defined purpose. These gaps make the
state-machine surface unsuitable even as a pure model checkpoint.

## P3 — bounded notes

### P3-1: Build-file conventions are not followed

The only Bluetooth CMake file lacks an SPDX header, target alias, public header
file set/install surface, repository warning helper, and public compatibility
metadata (`bluetooth_protocol/CMakeLists.txt:1-24`). Repair should follow the
existing service-module patterns rather than an isolated template.

### P3-2: Hardening claims exceed the unit

The service unit allows `AF_INET` and `AF_INET6` although B0 claims no network
socket need (`qindaqt-bluetooth-service.service:17`). It claims task and broad
kernel/personality/privilege hardening in `bluetooth-service.md:54-60`, but the
unit has no `TasksMax`, `ProtectKernelTunables`, `ProtectKernelModules`,
`LockPersonality`, `CapabilityBoundingSet`, or `RestrictSUIDSGID`. The eventual
unit should be generated, installed, and verified by a deployment test before
these claims land.

### P3-3: Documentation is orphaned and contradicts itself

Neither new page is in `mkdocs.yml`, the wiki index, or reciprocal navigation;
the module-boundary table has no four Bluetooth rows. Static doc validation
still reports the same 65 navigated documents as the parent, so it does not
validate these orphan pages. No ADR records the new process/trust/wire boundary,
despite `documentation-policy.md:47-60` requiring one.

The architecture introduction also says the D-Bus-activated service does not
access host D-Bus (`bluetooth-service.md:3-6`) while the same page says it
publishes D-Bus at lines 14-16 and the implementation calls registration APIs.
Current-vs-planned language must distinguish source-only design from executable
truth.

## Passing static observations

- Parent/merge base is exact public `9db68c4`; candidate has one parent.
- `git diff --check 9db68c4 f94353d`: PASS.
- `tools/check-source-shape`: PASS across 1,019 source files. Candidate maximum
  is 188 nonblank lines (`bluetooth_model.cpp`), so no 500-line decomposition
  review is triggered.
- `tools/validate-docs`: exits 0 for the 65 registered pages, but the two new
  unregistered Bluetooth pages are outside that navigated set.
- Exact candidate worktree remained clean throughout.

These checks provide no build, wire, test, activation, runtime, hardware, or
user-outcome evidence.

## Required repair boundary

Preserve `f94353d`, then repair on a non-amended descendant:

1. Re-ratify and implement the accepted B0 boundary: BlueZ/BluezQt owns device,
   pair/trust/key/profile truth; B0 exposes inventory, adapter power,
   owner-scoped discovery leases, and paired connect/disconnect. Defer Agent1
   pairing and trust prompts to the separately documented outcome and add the
   required ADR.
2. Define one canonical Bluetooth1 XML and generate/implement an explicit
   service adaptor plus exact-owner asynchronous client. Fix epoch generation,
   request IDs, timeout/uncertainty, snapshot clearing, stale replies,
   invalidation coalescing, and owner replacement.
3. Make protocol/model/client/service real repository targets with explicit
   public/private headers, installs, executable entry point, configured
   activation/unit files, root registries, package/linked-consumer checks, and
   current module/wiki/ADR navigation.
4. Repair fail-closed validation and state-machine invariants before any public
   codec publishes values. Make the fake an injected private backend port, not
   duplicated public storage.
5. Add non-vacuous tests: all exact signatures/round trips/hostile bounded
   decodes; capability/order/address/RSSI/state matrices; operation lineage and
   emitted revisions; private-bus activation/exact-owner A→B→A replacement;
   discovery lease owner loss; timeout/uncertainty; package/deployment/teardown.
6. Run strict Debug/Release and focused tests only after source repair, then
   request a different reviewer against one exact immutable commit. Do not
   advance `QQ-005.05` from ABSENT based on this rejected source checkpoint.
