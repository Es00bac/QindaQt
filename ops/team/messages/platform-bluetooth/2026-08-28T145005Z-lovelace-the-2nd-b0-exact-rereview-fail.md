# Lovelace the 2nd — Bluetooth B0 exact-repair rereview FAIL

- Time: 2026-08-28T08:50:05-06:00
- Reviewer: Lovelace the 2nd, OpenAI collaboration runtime; exact serving
  model and reasoning level unexposed
- Exact candidate: `e19d094c792d132d3d65129056281ca556415c0f`
- Tree: `75bbe5c4f71030d340761f4dbc392d28d12e3de7`
- Sole parent: `bbbe8b8f6f5e51033da857e3b0c6d38dc442fbb4`
- Repair base / merge base: `f94353d65c83d3c7b28888a2bd07aecd9f77ef4c`
- Worktree: clean, detached exact candidate
- Review mode: source/test/docs only; no product edit, configure, compile,
  CTest, D-Bus, BlueZ, rfkill, hardware, GUI/session, input, or configuration
- Verdict: **FAIL for integration and QQ-005.05 advancement**
- Findings: **P0/P1/P2/P3 = 0/9/5/3**

The repaired descendant materially fixes B0 authority: Pair/Trust/Untrust are
gone, Agent1 is explicitly deferred, build targets and an explicit service
object exist, documentation is navigated, and ADR-0037 is correctly reserved.
Those changes are preserved. They do not close the remaining product and
evidence blockers below.

## P0

None. No destructive or demonstrated host-mutating product behavior was found,
and this review invoked no runtime surface.

## P1 — blocking

### P1-1: The canonical Bluetooth1 signatures still disagree with the codecs

The C++ `Snapshot` fields are `u,t,t,u,u,s,s` before its arrays
(`src/services/bluetooth_protocol/include/qindaqt/services/bluetooth_protocol/bluetooth_types.h:102-111`;
writer at `src/services/bluetooth_protocol/src/bluetooth_dbus.cpp:113-122`),
so that prefix is `uttuuss`, not the documented/introspected `uutuuss`.
`Device` has three booleans after its class (`paired`, `connected`,
`rssiKnown`) at `bluetooth_types.h:88-99` and the writer at
`bluetooth_dbus.cpp:92-110`, so its tail is `ubbbn`, not `ubbbbn`.

The wrong forms are repeated in
`src/services/bluetooth_service/src/bluetooth_service_object_p.h:25-46`,
`src/services/bluetooth_service/data/org.qindaqt.Bluetooth1.xml:6-32`,
`docs/wiki/reference/bluetooth1-v1.md:27-34`, and the expected assertions at
`tests/services/bluetooth_protocol/tst_bluetooth_protocol.cpp:78-90`.
The signature test must fail against the registered codecs, and service
introspection cannot be canonical. Original P1-4 remains open.

### P1-2: The resident owner-watch registration uses an invalid D-Bus signature

`ResidentBluetoothService::start()` passes
`"name,old_owner,new_owner"` as the `QDBusConnection::connect` D-Bus signature
at `src/services/bluetooth_service/src/resident_bluetooth_service.cpp:51-60`.
That argument must be the type signature `sss`; names and commas are not a
valid D-Bus signature. `watched` therefore fails and `start()` returns
`InvalidConnection` before object/name registration. The same invalid value is
used for disconnect at lines 96-102. Every resident, transport, and activation
row depending on a started service is blocked.

### P1-3: Production drops its only initial deterministic inventory

The backend contract says `start()` returns its generation before the run can
publish (`src/services/bluetooth_model/include/qindaqt/services/bluetooth_model/adapter_backend.h:74-98`).
`DeterministicAdapterBackend::start()` instead emits inventory synchronously
before returning (`src/services/bluetooth_model/src/deterministic_adapter_backend.cpp:42-50`).
`BluetoothModel::start()` installs `m_backendGeneration` and `m_running` only
after that return (`src/services/bluetooth_model/src/bluetooth_model.cpp:104-131`),
so `acceptInventory()` drops the publication at lines 444-451. The shipped
composition root remains `Starting`; it cannot publish the promised
`Unavailable/no-adapter`. The activation assertion at
`tests/services/bluetooth_client/tst_bluetooth_activation.cpp:196-211` and the
empty replacement assertion in `tst_qt_bluetooth_transport.cpp:163-176` are
impossible as authored.

### P1-4: Restart-unique epoch fencing is not established

`advanceEpoch()` combines wall-clock milliseconds with only eight random bits
(`src/services/bluetooth_model/src/bluetooth_model.cpp:145-155`). Two processes
started in one millisecond can collide with probability 1/256, and a regressing
wall clock has no cross-process floor. This does not support the claim that a
restart can *never* reissue an earlier handle. Reused models additionally
advance only when `m_hasInventory` is true at lines 113-129; stop/start before
an accepted publication reuses the epoch. The latter is the normal production
path after P1-3.

### P1-5: Discovery leases survive authority boundaries and can exceed total bounds

Power-off clears connections and recomputes `discovering` but never clears
`m_leases` (`src/services/bluetooth_model/src/deterministic_adapter_backend.cpp:139-160`),
so power-on resurrects discovery. `stop()` also retains the lease table at
lines 53-56, allowing holds to cross backend runs. Acquisition enforces only
the per-adapter cap at lines 163-185; it never enforces the total 64-lease cap.
The model checks only its last published inventory
(`src/services/bluetooth_model/src/bluetooth_model.cpp:214-229`), so multiple
already-dispatched acquisitions can overrun that stale total before the backend
publishes. This contradicts the public backend contract at
`adapter_backend.h:43-49` and the service/protocol docs.

### P1-6: A client cannot activate an initially absent service

The transport performs only `GetNameOwner`; an error calls `setOwner({})`
(`src/services/bluetooth_client/src/qt_bluetooth_transport.cpp:96-115`). Because
the initial stored owner is already empty, `setOwner()` suppresses that event
at lines 129-148. The client remains `Starting` and issues neither
`StartServiceByName` nor a well-known-name method call. The activation test
masks the defect by explicitly activating the service before constructing the
client (`tests/services/bluetooth_client/tst_bluetooth_activation.cpp:179-199`).

### P1-7: Fetch failure leaves a dispatchable stale mutation snapshot

After a same-owner transport failure, malformed snapshot, regressing revision,
or equal-revision contradiction, the client publishes `Unavailable` and
schedules retry but retains `m_snapshot`
(`src/services/bluetooth_client/src/bluetooth_client.cpp:278-309`).
`beginOperation()` checks only nonempty owner plus snapshot presence, then
preflights the retained Ready snapshot and dispatches at lines 353-389. A
client explicitly reporting unavailable can therefore issue a mutation from
stale authority.

### P1-8: The focused client suite contains deterministic false expectations

Owner replacement queues an asynchronous completion through
`src/services/bluetooth_client/src/bluetooth_client_completion.cpp:21-37`, but
the test immediately requires synchronous delivery at
`tests/services/bluetooth_client/tst_bluetooth_client.cpp:151-155`.
Later, the first operation reply synchronously clears `m_operation`
(`bluetooth_client.cpp:446-485`), yet the test starts a new operation and
expects it to complete locally as Busy while `operationPending()` remains true
(`tst_bluetooth_client.cpp:303-331`). Both assertions contradict the exact
source and prevent the claimed focused pass even after service startup repair.

### P1-9: Required wire, owner-loss, installed-consumer, and isolation evidence is absent

The protocol "round trip" puts and retrieves the same C++ value from
`QVariant` (`tests/services/bluetooth_protocol/tst_bluetooth_protocol.cpp:93-107`);
it does not invoke the D-Bus writers/readers. The oversize test manually sets
`wireValid=false` at lines 260-278 rather than decoding an oversized wire
array. No private-bus row disconnects an actual lease-holding caller and proves
release. The service unit instead registers on ambient
`QDBusConnection::sessionBus()` at
`tests/services/bluetooth_service/tst_bluetooth_service.cpp:45-56`, violating
the private-bus test boundary. The activation row synthesizes a descriptor at
`tests/services/bluetooth_client/tst_bluetooth_activation.cpp:54-103`; no
staged installed descriptor/unit/XML or linked installed consumer is tested in
any Bluetooth CMake registry. These were explicit original repair requirements
and were nevertheless claimed complete in the handoff.

## P2 — serious repair findings

### P2-1: Lease inventory contradictions and caller-loss identity are not fail closed

`leaseBoundsRespected()` validates caller text, address form, refcount, and sums
only (`src/services/bluetooth_model/src/bluetooth_model.cpp:402-423`). It accepts
leases for nonexistent adapters, duplicate caller/adapter entries, and lease
tables contradicting the adapter's `discovering` flag. If the P1-2 signature is
fixed, `onNameOwnerChanged()` still releases `oldOwner` for *any* well-known
name relinquished without replacement (`resident_bluetooth_service.cpp:116-130`),
even while that unique caller remains connected. It must identify actual unique
name disappearance rather than every alias release.

### P2-2: Connect omits the already-connected state promised by the repair

Model preflight validates pairing and adapter power but not existing connection
state (`bluetooth_model.cpp:241-256`); the backend likewise returns success for
an already-connected device (`deterministic_adapter_backend.cpp:204-225`).
Anika's original P2-4 explicitly required connected/already-connected policy,
and the repair handoff claimed every connection state was validated.

### P2-3: Public lifetime documentation and decomposition remain incomplete

`BluetoothModel::snapshot()` exposes a reference to mutable internal storage at
`src/services/bluetooth_model/include/qindaqt/services/bluetooth_model/bluetooth_model.h:38`
without stating when publication invalidates that reference. The source-shape
checker reports `src/services/bluetooth_model/src/bluetooth_model.cpp` at 525
nonblank lines, crossing the mandatory decomposition-review threshold; no
decomposition review/ADR rationale accompanies it.

### P2-4: Transport completion and retry behavior contradict its public contract

`BluetoothTransport` promises every completion asynchronously
(`src/services/bluetooth_client/include/qindaqt/services/bluetooth_client/bluetooth_transport.h:12-15`),
but `QtBluetoothTransport` directly emits failure from `fetchSnapshot()` and
`submitOperation()` at `qt_bluetooth_transport.cpp:157-162,181-187`. Client
retry is a fixed 200 ms loop (`bluetooth_client.cpp:106-112,259-263`), not the
bounded timeout/backoff behavior recorded in the module boundary.

### P2-5: The accepted base user outcome has no battery/role representation

The accepted Bluetooth1 plan includes device battery/role status where BlueZ
provides it
(`ops/team/messages/platform-services/1787853847-samira-cole-plan-handoff.md:263-292`).
The fixed Device v1 value has neither field (`bluetooth_types.h:88-99`), and the
architecture page does not mark this accepted part as deferred. Adding it later
would now require a schema revision rather than only replacing the backend.

## P3 — bounded notes

### P3-1: The handoff's source-shape statement is false

The handoff says the largest new source is below 500 nonblank lines. The exact
repository checker passes only because 500 is a review threshold rather than a
hard failure and reports `bluetooth_model.cpp` at 525. Future handoffs must
quote the actual warning rather than converting exit zero into absence.

### P3-2: Current-main integration needs two explicit additive resolutions

Against current public `50742fed62427c2f848ac13df94c488366e136a0`, a
read-only `git merge-tree --write-tree` reports content conflicts in
`docs/wiki/adr/index.md` and `mkdocs.yml`; the source/test root registries merge
cleanly. This is not a candidate correctness defect, but the manager must retain
both public ADR/nav additions when a repaired candidate eventually passes.

### P3-3: The user unit cannot order after the system BlueZ unit as documented

`qindaqt-bluetooth-service.service.in:4` puts `After=bluetooth.service` in a
user unit. User-manager ordering cannot order a system-manager BlueZ unit, so
the statement in `docs/wiki/architecture/bluetooth-service.md:83-88` is not an
effective dependency. The service should tolerate BlueZ absence by design and
the unit/docs should state the real boundary.

## Passing static observations

- Candidate, tree, sole parent, repair base, and clean detached worktree match.
- The 60-path repair manifest SHA-256 is
  `f3e1cbb40267458937561a080f13bfd39f19da179168b7ed77c622c5c973b3ce`.
- The nine-path ADR descendant manifest SHA-256 is
  `e42ba497770cbcf8323d1b8994976319e8d47dc851cd6deb0135359923506fcb`.
- `git diff --check f94353d e19d094`: PASS.
- `python3 tools/check-source-shape`: exits 0 across 1,044 files, with the
  explicit 525-line decomposition warning above.
- `python3 tools/validate-docs`: PASS across 66 navigated Markdown documents.
- Authority rescope, Agent1 deferral, ADR-0037 links, module navigation, root
  build registrations, explicit adaptor shape, hardened unit fields, and
  bounded value validators are preserved improvements.

These checks are not compile, test, D-Bus, activation, installed-package,
BlueZ, hardware, or user-outcome evidence.

## Required next repair

Preserve `e19d094`, then create one non-amended descendant that repairs all P1
and closes or truthfully bounds P2/P3. At minimum: derive signatures from the
actual types and make XML/class-info/reference/tests identical; install a valid
unique-owner watch; make initial publication generation-safe; establish robust
process/reuse epoch uniqueness; clear and globally bound leases; activate an
absent service; revoke stale mutation authority; correct the client tests; add
real QDBusArgument hostile round trips, private caller-loss, private service,
and staged installed-consumer/deployment gates; decompose or record the model
review; and update the wiki in the same change. The same Lovelace persona is
available to rereview only the exact repaired commit.
