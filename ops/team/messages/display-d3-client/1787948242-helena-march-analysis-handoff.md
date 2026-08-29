# Helena March — Handoff: Display1 D3 client/coordinator analysis (implementation- and review-ready)

- Time: 2026-08-28T20:17:22Z
- Author: Helena March, Anthropic Claude Code `claude-fable-5`, reasoning high. Analysis only; I edited nothing outside my profile and my replies in this thread.
- Evidence basis: read-only inspection of `/home/cabewse/work_SPaC3/container-wm-workers/display-d3-kimi-nyra` (dirty tree on base `146fc483`) as on disk between 20:05Z and 20:20Z today, against manager `0760e08`, ADR-0016, `docs/wiki/architecture/display-service.md`, `docs/wiki/reference/display1-v1.md`, the D2 service sources/tests, and the audio/settings client precedents. No build, test, bus, or display was run; every claim below is static and cites file:line as observed. If Pavel has since changed a cited line, re-check the finding rather than the line number.
- Supersedes nothing; extends `1787948023-helena-march-midpoint-blocking-findings.md`.

## 1. Where the partial stands

| Path | Non-blank lines | State |
| --- | --- | --- |
| `src/services/display_client/include/.../display_transport.h` | 52 | Complete abstract seam (Nyra) |
| `.../qt_display_transport.h` + `src/qt_display_transport.cpp` | 54 / 211 | Complete (Nyra), near-copy of `audio_client/src/qt_audio_transport.cpp`; inherits that file's raw-decode shortcut |
| `.../client.h` + `src/display_client.cpp` | 131 / **493** | Complete (Pavel). 493 is seven lines under the 500-line decomposition-review trigger in `tools/source-shape.json` |
| `.../display_coordinator.h` + `src/display_coordinator.cpp` | 85 / 189 | Complete (Pavel); **no tests reference it** |
| `tests/services/display_client/**` | 5 binaries, 18 cases | Written (Tara); will not compile under `-Werror`; see §7 |
| `src/CMakeLists.txt`, `tests/CMakeLists.txt` | +1 each | Additive; both files gained +9 lines on the manager branch since base, `testing-harness.md` gained +74 — trivial rebase, but the harness page must be edited against the manager version |
| `.omc/` | untracked | Must not enter the candidate |

Service facts the client design must respect (from `display_service_model.cpp:326-360` and `display_service_object.cpp:55-80`):
- The service never returns `OperationStatus::Uncertain`; `Uncertain` is always client-originated (transport failure, timeout, owner loss, supersession, stop).
- `Preview` returns `Accepted` when the machine enters **`Applying`**, not `AwaitingConfirmation`. Confirmation is legal only after apply callback + observation. A `Confirm` sent immediately after the Preview reply is rejected `InvalidTransition`.
- `Confirm` returns `Succeeded`; a no-op `Stage` returns `Succeeded` with diagnostic `no-op` and stages nothing.
- Results echo `kind`, bounded `transactionId`, and `initiatingEpoch/initiatingRevision` = the service snapshot at command time (may be newer than the client's when the result is `Rejected` StaleRevision/UnknownTransaction).
- `Changed(epoch, revision, available)` is emitted on every accepted state change, including transaction state transitions; `GetSnapshot` carries `transactions[0]` with `state`, `reason`, `deadlineMonotonicMilliseconds` (service clock), `revertAttempt`.
- When no inventory is accepted, methods fail with `org.qindaqt.Display1.Error.Unavailable`.

## 2. Findings (file:line, severity, repair)

### P0

**F1 — Raw-decoded, semantically unvalidated payloads are published.**
`qt_display_transport.cpp:171,177` and `:223,229` use `QDBusPendingReply<T>::value()` (raw `operator>>`). The documented adapter boundary is `Display::decodeSnapshotArgument` / `decodeOperationResultArgument` (`display_dbus.h:19-30`: exact static signature → temporary → `validateSnapshot`/`validateOperationResult` → replace). `Client::acceptSnapshotReply` (`display_client.cpp:349`) and `acceptOperationReply` (`:393`) trust `wireValid`, which the raw decoder clears only on list-cap overflow (`display_dbus.cpp:37-47,266-274`). Text byte limits, pixel/coordinate/scale/refresh bounds, protocol version, 32-byte fingerprint, at-least-one-enabled/exactly-one-primary, and result success/error consistency are never checked. Precedent: `audio_client.cpp:293,471` validates client-side.
Failure: a misbehaving or replaced owner returns an oversized/out-of-bounds snapshot → `snapshotChanged` publishes it to Settings/shell → `stage()` builds candidates on it.
Repair: (a) transport: `const QDBusMessage m = watcher->reply(); if (m.type() != ReplyMessage || m.arguments().size() != 1) → malformed-reply; Display::Snapshot tmp; const auto r = decodeSnapshotArgument(qvariant_cast<QDBusArgument>(m.arguments().at(0)), tmp); if (!r.accepted) → emit snapshotReply(owner, id, false, {}, "malformed-reply:" + r.reasonCode)`; same for `OperationResult`. (b) Client: run `Display::validateSnapshot` / `validateOperationResult` again before accepting (belt and braces; this is what a fake-transport row can prove). Drop the `wireValid` checks — they are subsumed.

**F2 — Transport emits completions synchronously, violating its own contract.**
`display_transport.h:14-15` promises asynchronous completion; `qt_display_transport.cpp:160-163` and `:211-214` emit inline. Consequences: `Client::start()` (`display_client.cpp:97-99`) fetches with an empty owner → inline failure → `Starting→Unavailable("owner-unavailable")` flicker (`:334-341`) → 2 s poll (`:464`) that never reaches the bus; `Coordinator::begin()` (`display_coordinator.cpp:71-72`) can observe `stateChanged(Unavailable)` re-entrantly before publishing `Staging` (`Unavailable→Staging` order, then a deferred Uncertain completion).
Repair: in the transport route both precondition failures through `QMetaObject::invokeMethod(this, [=]{ Q_EMIT ...; }, Qt::QueuedConnection)`. Keep the Client's own deferral at `:497` as the second fence. Add `d->running` checks inside the reply lambdas (`:170-179,:222-231`) so a stopped transport emits nothing.

**F3 — "No owner" after initial resolution is never reported.**
`setOwner` (`qt_display_transport.cpp:129-132`) returns early on equality, so an errored/empty `GetNameOwner` reply (`:114`) emits nothing when `d->owner` is already empty. The Client reaches `Unavailable` today only through F2's accident. `Client::acceptOwner` (`display_client.cpp:265-267`) has the same equality short-circuit.
Trap: fixing F2 alone, or guarding `requestSnapshot()` on an empty owner, leaves a client started while Display1 is not running in `Starting` forever with no timer armed.
Repair: transport emits `ownerChanged(d->owner)` unconditionally when the initial query completes (only that path; `onServiceOwnerChanged` may keep the dedup). Client: in `acceptOwner`, an empty owner always publishes `Unavailable("owner-unavailable")` (publishState already dedups); `requestSnapshot()` returns early when `m_owner.isEmpty()`; `scheduleRefetch()` does not arm the poll without an owner. `Starting` then means exactly "owner resolution or first fetch in flight".

### P1

**F4 — No activation.** The transport addresses only unique names (`:165-167,216-218`); nothing calls `StartServiceByName` for the activatable `org.qindaqt.Display1` (`display1-v1.md` "Resident D2 wire surface"; ADR-0016 consequences name activation a D2 release requirement). Precedent: `qt_settings_transport.cpp:305-322` and `settings_client.cpp:409-419` ("a successful StartServiceByName reply is not an owner baseline"). Decision required (see §8): add `requestActivation()` + `activationFinished(bool)` to `DisplayTransport`, invoked once at `start()` and after a resolved-empty owner with bounded backoff (2 s doubling to 30 s), owner baseline still only from `GetNameOwner`/`NameOwnerChanged` — or document explicitly which session component starts the service and that D3 clients wait passively. I recommend adding it.

**F5 — Reply payload lineage is recorded but never checked.** `client.h:84-90` stores `epochAtSubmit/revisionAtSubmit`; `acceptOperationReply` (`display_client.cpp:368-406`) gates on `requestId` only and ignores the reply `owner` (`:371`; same at `:322` for snapshots). On the real transport this is redundancy; on a hostile or buggy transport it is the only thing that stops a Preview result being attributed to a Stage or a different transaction's result being accepted.
Repair (precise, because the service legitimately returns a newer initiating lineage on `Rejected`): complete as `Uncertain("lineage-mismatch")` when `owner != m_owner`, or `result.kind != op.kind`, or (`result.transactionId` non-empty and `!= op.transactionId`), or (status is `Accepted`/`Succeeded` and `result.initiatingEpoch != op.epochAtSubmit`). Do not require revision equality. For snapshots: reject when `owner != m_owner`.

**F6 — Exactly-once breaks on `stop(); start();` in one turn.** `stop()` (`display_client.cpp:121-129`) clears the queue then enqueues the promised `client-stopped` result; `start()` (`:90`) clears it again before the zero-timer fires. The requestId never completes; a Coordinator in `Cancelling` hangs with its deadline timer stopped (`display_coordinator.cpp:193`).
Repair: remove `cancelQueuedOperationCompletions()` from `start()`, and from `stop()` too — queued entries are already-final local results; ids are process-monotonic (`m_nextRequestId` is never reset, correctly), so nothing collides. Keep the drop-on-destruction semantic only (the `singleShot(0, this, …)` context already guarantees it). Update the header contract at `client.h:29-32` accordingly.

**F7 — `Coordinator::cancel()` during `Confirming` reports `confirmed=false` for a possibly committed transaction.** `display_coordinator.cpp:87-98` allows cancel in `Confirming`; `beginCancel` → `Client::cancel` supersedes the in-flight Confirm as `Uncertain` (`display_client.cpp:251-253`); the service may already have cleared the journal (`Confirm` → `Succeeded`); the Cancel then returns `Rejected(UnknownTransaction|InvalidTransition)` and `:163-169` finishes `confirmed=false` unconditionally. The `confirm-uncertain` path (`:157-158`) has the same hole.
Repair: `cancel()` returns `false` in `Confirming` (confirm is the client's point of no return); replace `transactionFinished(id, bool confirmed, reason)` with a closed `Outcome { Confirmed, Reverted, Uncertain, NoOp }`; finish `Uncertain` whenever Preview or Confirm completed `Uncertain` and the follow-up Cancel did not return `Accepted`/`Succeeded`. Optional resolution of `Uncertain`: on the next accepted snapshot with no `transactions[]` entry for our id, compare `liveFingerprint` with `DisplayTopology::canonicalFingerprint` of the normalized candidate (the module already links `QindaQt::DisplayTopology` and uses nothing from it) → `Confirmed`; equal to the pre-image fingerprint → `Reverted`; otherwise stay `Uncertain`.

**F8 — Coordinator enters `AwaitingConfirmation` on the Preview reply and runs its own revert timer; it also ignores client loss there.**
- `:140-143` treats Preview `Accepted` as awaiting confirmation. Per §1 the server is in `Applying`; a `confirm()` issued before apply+observe completes is rejected `InvalidTransition` and `:159-160` then finishes the coordinator as Cancelled while the server preview is still live and will time out on its own.
- The 15 s client timer (`:142`, default `:80`) starts at the Preview reply; the server's 15 s window starts later, at `AwaitingConfirmation`. The client timer therefore fires first in every real run and cancels a preview the user still had time to confirm. ADR-0016: consumer countdowns "are projections, not timer or mutation authority".
- `:100-119` ignores `Unavailable`/`Stopped` in all active states; in `AwaitingConfirmation` no operation is in flight, so nothing resolves the coordinator until the timer fires and its cancel is locally rejected.
Repair: subscribe to `Client::snapshotChanged`; enter `AwaitingConfirmation` only when `transactions[0].transactionId == m_transactionId && state == AwaitingConfirmation`; leave it when the summary disappears or changes state (server timeout/lock/suspend/topology → `Reverted` with the summary's `reason`; the service emits `Changed` on each transition so the client refetches). On `Unavailable`/`Stopped`/owner change/epoch change during any active state, finish `Uncertain("lineage-lost")` immediately (the server reverts or discards on its own). If a client-side timer is retained, name it a rescue, default it above the server window (server 15 s + apply 5 s + observe 2 s + grace ≥ 25 s), and document that it never preempts the server.

### P2

**F9 — Local reasons collapse into `ErrorCode::CompositorUnavailable`** (`display_client.cpp:15-30`): `owner-changed`, `service-unavailable`, `client-stopped`, `superseded-by-cancel`, `no-snapshot`, `client-not-running` all map to a code the protocol defines as a compositor condition. `ErrorCode` is closed (`display_types.h:74-91`), so keep `diagnostic` as the client discriminator, but map local preconditions (`no-snapshot`, `client-not-running`, `operation-pending`) to `InvalidTransition`/`TransactionActive`, and document the mapping table in `client.h` as a compatibility contract with one pinning test row.

**F10 — `Client::snapshot()` returns a default-constructed snapshot when none is held** (`:167-170`): `wireValid=true`, revision 0, empty epoch — the exact "plausible revision-zero snapshot" the service is forbidden to return. Return `std::optional<Display::Snapshot>` (or keep `hasSnapshot()` and assert).

**F11 — `stateChanged`/`operationCompleted` argument types are declared metatypes but never registered** (`client.h:131`, `display_coordinator.h:85`; no `qRegisterMetaType` in either constructor). Direct connections work; any queued connection (QML consumer, cross-thread receiver) fails at runtime. Register in the constructors.

**F12 — Transport never observes bus disconnection.** No handling of the local `Disconnected` signal (precedent `qt_settings_transport.cpp` `handleBusDisconnected`). After daemon loss the owner string stays set and every call fails `owner-unavailable`; the client still reaches `Unavailable` via reason code, so this is degraded truth rather than a lie. Map `QDBusError::TimedOut` alongside `Timeout`/`NoReply` at `:21`.

**F13 — Candidate direction is unvalidated.** `Client::stage` (`display_client.cpp:182-205`) forwards any candidate. Run `Display::validateCandidate` (`display_validation.h:23`) first and reject locally `Rejected(InvalidCandidate, "invalid-candidate")`; this keeps oversized candidates off the wire and gives a fake-transport row an observable.

**F14 — Stage `Succeeded`/`no-op` is treated as staged** (`display_coordinator.cpp:132-135`): the coordinator then previews a non-existent transaction and finishes `Cancelled("unknown-transaction")`. Finish `NoOp` instead (see F7 outcome enum).

**F15 — Polling while the owner has declared `available=false`.** After `Changed(…, false)` the in-flight fetch fails `service-unavailable` and arms the 2 s poll (`:334-341`), which then hits the bus every 2 s until `Changed(…, true)`. Poll only on transport errors; rely on `Changed` for service-declared unavailability.

**F16 — Dropped hybrid/regressed snapshots are silent** (`:356-360`): the client stays `Ready` with no signal. Publish `Degraded("snapshot-rejected")` (next good reply restores `Ready`) so a misbehaving owner is observable and testable.

**F17 — Source shape and packaging.** Any P0 repair pushes `display_client.cpp` past 500 non-blank lines; split before that (e.g. `src/display_client_results.cpp` for `acceptsSnapshot`, `errorCodeForReason`, `localResult`, and the new validation helpers, with a private header). `QindaQt::DisplayTopology` is a PUBLIC link with no use — either use it (F7) or drop it from the installed link interface. Header naming: `client.h` is the only one without the `display_` prefix; harmless, but the installed-consumer row must include all four headers.

## 3. Contract table (what each header must state; gaps marked)

| Type | Ownership | Lifetime | Threading | Error behaviour | Compatibility | Gaps in current header |
| --- | --- | --- | --- | --- | --- | --- |
| `DisplayTransport` (`display_transport.h`) | Borrowed by `Client`; QObject parent optional | Must outlive every `Client` bound to it | One thread, the constructing thread | Completion always asynchronous, at most once per `requestId`, echoes exact submitted `owner`/`requestId`; reason codes are a closed set | Signal signatures and reason-code vocabulary are public API | Closed reason-code list not written (`owner-unavailable`, `service-unavailable`, `transport-timeout`, `malformed-reply`, `transport-error`); requestId uniqueness is caller-owned and unstated; F2 violates the async clause; no activation seam (F4) |
| `QtDisplayTransport` | Value handle to a named `QDBusConnection` (`qt_display_transport.h:19-21`) | Connection must stay registered; in-flight watchers parented to the object | Constructing thread only | Normalized errors (`:19-36`); after `stop()` late watcher completions still emit (`:170-179`) | Binds `Changed` to the unique owner (`:141-146`), which is the correct owner/payload binding | Add: "completions after `stop()` are suppressed"; "destructor calls `stop()` and may emit `ownerChanged({})`, so destroy bound Clients first" |
| `Client` (`client.h:25-32`) | Borrows transport; owns snapshot/operation state | Callable `start/stop` cycles; ids monotonic across cycles | One thread | Local rejections and uncertainties are typed `OperationResult`s delivered exactly once (F6 breaks this on stop/start) | `ClientState`, reason codes, and the `ErrorCode` mapping are public | Document the state meanings (`Starting` = resolution/first fetch in flight; `Unavailable` = no owner or owner said unavailable; `Degraded` = owner present, last transport/decode failure, LKG may be held; `Busy` = op pending; `Ready` = snapshot held, no op), the mapping table (F9), and that `snapshot()` is only meaningful when `hasSnapshot()` (F10) |
| `Coordinator` (`display_coordinator.h:25-34`) | Borrows `Client` | Must not outlive its `Client` | One thread | Fail-closed returns; `transactionFinished` once per `begin()` | `CoordinatorState`, outcome, and reason codes public | Replace `bool confirmed` with the closed outcome enum (F7); state the server-projection rule and the rescue-timer rule (F8); bind the transaction to `(owner, epoch)` at `begin()` |

## 4. Behaviour-by-behaviour verdicts

- **Owner/payload binding.** Correct at the transport: match rule on the unique owner (`qt_display_transport.cpp:141-146`), calls addressed to the unique owner, `invalidated` carries the owner and the client drops foreign owners (`display_client.cpp:301-303`). Incomplete at the reply boundary: owner echo unchecked (F5).
- **Real A/B/A owner lineage.** A bus daemon never reuses unique names; the only same-name A/B/A is one connection re-registering, which arrives as two `NameOwnerChanged` events and is already handled as two owner changes (`qt_display_transport.cpp:118-127`, `display_client.cpp:260-286`: reset snapshot, complete op Uncertain, refetch under a new requestId). The real A/B/A hazards are (i) a late reply from owner A arriving after B was bound — fenced by `m_fetchRequestId`/`m_operation->requestId` because `acceptOwner` allocates new ids; and (ii) an epoch replay through the same owner — accepted by design (`:44-46`) because epochs are unordered; the consumer's fence is the candidate lineage check (`:197-200`). Optional hardening: record the epoch from the last `Changed(…, true)` on the current owner and reject a fetched snapshot carrying a different epoch when one was announced (bus FIFO guarantees the announcement precedes the reply; bound the announced epoch with `isBoundedText`).
- **Coalesced invalidation.** Correct: `requestSnapshot` folds N `Changed` during one flight into one follow-up (`:446-457`, `:342-345`, `:362-365`). Negative control: N invalidations during one flight → exactly two `fetchSnapshot` calls.
- **Atomic LKG.** Correct: whole-value assignment then signal (`:440-444`); regression/hybrid replies leave the held snapshot untouched (`:39-54`). Owner change and `available=false` deliberately drop LKG (`:270`, `:306`) — right, revisions cannot be ordered across owners. Gap: silent drop (F16).
- **Cancellation / timeout / late callback / exactly-once.** Late and duplicate replies are dropped by requestId (`:327-329`, `:381-383`); timeouts complete `Uncertain` and never resend (`:421-428`); `cancel()` supersedes an in-flight op (`:251-253`) — acceptable at the Client, dangerous when the Coordinator uses it in `Confirming` (F7). Exactly-once holds except F6. Invariant to keep: `m_nextRequestId` is never reset in `start()` (`:82-100` — correct today; make it an `AGENT-GUARD`).
- **Preview→confirm/revert deadline and service-loss machine.** Coordinator is premature on `AwaitingConfirmation`, owns a competing timer, and is blind to client loss and lineage change in that state (F8); confirm/cancel truth is misreported (F7). Recommended machine: `Idle → Staging → Previewing(server Applying/Observing) → AwaitingConfirmation(server-projected) → Confirming → Confirmed`, with `→ Cancelling → Reverted` from any active state via explicit `cancel()`, and `→ Uncertain` from Uncertain Preview/Confirm without a Succeeded cancel, from owner/epoch change, and from client `Unavailable`/`Stopped`.
- **Transaction-token lineage.** The service scopes `transactionId` to its current epoch and machine; owner replacement discards the machine. Coordinator must capture `(owner, epoch)` at `begin()` and finish `Uncertain("lineage-lost")` when either changes; Client-level F5 covers the reply side.
- **Malformed / oversized payloads.** Inbound: F1. Outbound: F13. `Changed` arguments are unbounded strings; only bound them if the announced-epoch fence is adopted.
- **Installed / source-policy / package seams.** CMake matches the service module (STATIC + `FILE_SET HEADERS` + `install(EXPORT QindaQtTargets)`, `CMakeLists.txt:5-41`); LGPL sources / GPL tests match the repo. Rebase note in §1. Installed-consumer row must compile a TU including all four headers against the staged prefix.

## 5. Smallest repair order (each step leaves the tree buildable)

1. Transport: F2 (queue inline failures; gate reply lambdas on `d->running`), F3 (unconditional `ownerChanged` after initial resolution), F12 (`TimedOut`). No header change.
2. Transport: F1 safe decoders. No header change.
3. Client: F3 (empty-owner handling in `acceptOwner`, no fetch/poll without owner), F1 (re-validate), F6 (stop clearing the queue), F5 (lineage checks), F13 (`validateCandidate`), F11 (`qRegisterMetaType`). Split the file (F17) as part of this step.
4. Client header: F10 (`std::optional`), F9 (mapping table + state semantics as `AGENT-CONTRACT`), F6 wording.
5. Coordinator: F8 (snapshot-projected `AwaitingConfirmation`, loss/lineage handling, rescue timer ≥ server window), F7 (outcome enum, no cancel in `Confirming`), F14 (`NoOp`).
6. Decision F4 (activation) — smallest additive change to `DisplayTransport` if adopted; do it before Tara's private-bus row so the row can prove "no owner → activation attempt → owner bound".
7. Docs: `display-service.md` (D3 section: client states, coordinator projection rule, what D3 does not prove), `display1-v1.md` (acceptance matrix rows), `testing-harness.md` (private-bus selector), ADR-0016 consequences only if the activation decision changes an accepted statement.

## 6. Invariants and negative controls

| Invariant | Negative control (must fail if the invariant is broken) |
| --- | --- |
| Every `stage/preview/confirm/cancel` call completes exactly once with its returned `requestId`, including after `stop()` and across `stop();start();` | Fake transport never replies; call `stop()` then `start()` in the same turn; expect one `client-stopped` Uncertain completion for the pending id and none for others |
| No completion is delivered synchronously from within a public call | Fake transport replies inline from `submitStage`; the completion signal must arrive after `stage()` returns |
| A reply is accepted only for the exact pending `requestId`, owner, kind, transactionId, and (on success) submit epoch | Fake replies with wrong id / wrong owner / wrong kind / wrong id string / wrong epoch → dropped or `Uncertain("lineage-mismatch")`, never `Accepted` |
| Held snapshot never moves backwards or splices | Deliver rev 3 then rev 2, then rev 3 with different contents → exactly one `snapshotChanged`; `snapshot()` unchanged; state `Degraded("snapshot-rejected")` |
| Unvalidated wire content is never published | Fake reply with 33 outputs / 129-byte stable ID / scale 3.5 / 31-byte fingerprint / protocolVersion 2 / `Succeeded` with non-None error → no `snapshotChanged` / `Uncertain("malformed-reply")` |
| Empty owner is always reported and never polled | Fake `ownerChanged({})` after start → `Unavailable`; zero `fetchSnapshot` calls until a non-empty owner |
| N invalidations during one fetch produce exactly one follow-up fetch | Emit `invalidated` ×5 before replying → exactly 2 `fetchSnapshot` calls |
| Owner change discards LKG, completes the in-flight op Uncertain, and fences the old reply | `ownerChanged(B)` while op and fetch are in flight from A; late A replies for the old ids → dropped; new fetch id issued for B |
| Coordinator enters `AwaitingConfirmation` only from a snapshot whose `transactions[0]` says so | Preview `Accepted` with a snapshot still showing `Applying` → state stays `Previewing`; `confirm()` returns false |
| Coordinator never reports `Confirmed` or `Reverted` when the last forward op was Uncertain and cancel did not succeed | Preview times out; cancel returns `Rejected` → outcome `Uncertain` |
| Coordinator never cancels a preview before the server's window can expire | Rescue timer default ≥ 25 s; a row with server-projected `AwaitingConfirmation` and a 20 s wait must not observe a client-issued Cancel |
| `m_nextRequestId` is process-monotonic | Ids returned after `stop();start();` are strictly greater than any id returned before |

## 7. Tara's five binaries — verdict and required rows

Verdict: as written, none of the 18 cases proves its named behaviour, and the suite does not compile under strict warnings.

Compile blockers (`-Werror` via `cmake/QindaQtCompiler.cmake:24`): `tst_display_client_owner_lineage.cpp:83,92,100,108` designated initializer `.errorCode` — the member is `error` (`display_types.h:212`); unused parameters in the mock slots `:75-77,:88,:96,:104`; unused `serviceUniqueNameA` `:153`; `stateChanges` set but never read `:163-165`; unused `requestId` at `tst_display_client_operation_state.cpp:94,125,157`.

Structural: `MockDisplayService1` (`tst_display_client_owner_lineage.cpp:25-116`) registers with `registerObject(path, this)` (default `ExportAdaptors`, exports nothing), names methods `getSnapshot/stage/...` (QtDBus is case-sensitive; the wire names are `GetSnapshot/Stage/...`), returns `QVariant` (wire type `v`, not the struct signature the client requires), and emits `changed` on interface `local.MockDisplayService1`, not `org.qindaqt.Display1.Changed`. No row can reach `Ready`. Row 1 also never re-registers after `unregisterService()` (`:177-186`), and its premise is not a bus behaviour (unique names are never reused). The other four files register no service at all; their assertions are either/or (`tst_display_client_service_state.cpp:53-60,93-99,175-176,217-224`) or tautologies (`tst_display_client_operation_state.cpp:65,98,128,160`; `:201-203` only checks ids are non-zero), and `lateReplyAfterTimeoutIsIgnored` has no late reply to ignore. The Coordinator has no test.

Recommended oracle A — deterministic fake transport (no bus, no daemon): a `FakeDisplayTransport : DisplayTransport` in `tests/services/display_client/support/` that records `{kind, owner, requestId, transactionId, candidate}` and lets the row emit `ownerChanged/invalidated/snapshotReply/operationReply` at will, including inline from inside `fetchSnapshot()`. This is the injection seam Nyra designed (`display_transport.h:12-15`), not a duplicate of production code, and it is the only way to exercise hostile replies, late replies, inline replies, and exactly-once deterministically.

Recommended oracle B — real service on a private bus: compose `ResidentDisplayService` with `FakeInventorySource`, `FakeTransactionPort`, and an elapsed clock exactly as `tests/services/display_service/tst_resident_display_service_private_bus.cpp:92-107`, linking `QindaQt::DisplayService`; copy the two fakes and the `frame()/output()` builders from `tests/services/display_service/support/display_service_test_support.h` into Tara's own support header (her exclusive path). This gives real `Unavailable` errors, real `Changed`, real Stage/Preview/Confirm/Cancel results, owner replacement (start a second resident on a second connection), and — with `service.model()->safetyChanged(Safe)` plus `FakeTransactionPort` completion — a genuine server-side `AwaitingConfirmation`.

Required named rows (replace the current five):

| CTest row | Oracle | Cases that must exist |
| --- | --- | --- |
| `qindaqt.display-client-lineage` | A | late reply after owner change dropped; duplicate reply dropped; wrong owner / kind / transactionId / epoch → `lineage-mismatch`; epoch A→B→A on one owner accepted only with a matching announced epoch (if fence adopted) and stale candidate rejected `StaleRevision`; revision regression and same-revision hybrid dropped with `Degraded("snapshot-rejected")` |
| `qindaqt.display-client-publication` | A | one `snapshotChanged` per accepted reply; exact-equal redelivery accepted silently; 5 invalidations → 2 fetches; `available=false` drops LKG → `Unavailable`; empty owner never fetches; hostile payload matrix from §6 never publishes |
| `qindaqt.display-client-operations` | A | exactly-once across timeout, cancel supersession, owner change, stop, `stop();start();`; inline reply arrives after the call returns; second `stage()` while pending → `Rejected(TransactionActive,"operation-pending")`; `validateCandidate` rejection; timeout → `Uncertain(Timeout,"transport-timeout")` with no resend (fake records exactly one submit) |
| `qindaqt.display-client-coordinator` | A | full stage→preview→(snapshot shows `AwaitingConfirmation`)→confirm→`Confirmed`; preview `Accepted` with snapshot `Applying` keeps `Previewing` and rejects `confirm()`; server summary vanishing → `Reverted(reason)`; preview Uncertain + cancel Rejected → `Uncertain`; `cancel()` false in `Confirming`; no-op stage → `NoOp`; client `Unavailable` in `AwaitingConfirmation` → `Uncertain("lineage-lost")`; rescue timer does not fire before the server window |
| `qindaqt.display-client-private-bus` | B | no owner → `Unavailable` (and activation attempt if F4 adopted); resident start → `Ready` with the real epoch; `Changed` after `publish(frame(2))` → revision 2; resident stop → `Unavailable`, second resident on another connection → new epoch, old-epoch candidate rejected; with `Safe` + fake port completion: stage→preview→observe `AwaitingConfirmation` in the snapshot→confirm `Succeeded`; teardown leaves no daemon or fixture root |

Row B keeps the existing labels (`private-dbus;isolated-runtime`, `RUN_SERIAL`); rows A need no `dbus-daemon` and should not carry `find_program(... REQUIRED)`.

## 8. Tempting incorrect fixes

- Guarding `requestSnapshot()` on an empty owner without F3 → permanent `Starting` when Display1 is not running.
- Shortening the D-Bus call timeout to match `m_requestTimeoutMs` → the client would then see two different timeout paths; the client timer is the authority, leave the wire timeout at default or longer.
- Retrying a timed-out Preview/Confirm "because it probably never arrived" → forbidden by ADR-0016 (uncertain forward request is never replayed).
- Making the Coordinator's 15 s timer "match the server" by starting it later → still a second authority; project the server summary instead.
- Recomputing the topology fingerprint in the Client to "validate" snapshots → topology policy is the service's; the client checks bounds and length only (fingerprint comparison belongs to the optional Coordinator resolution in F7).
- Resetting `m_nextRequestId` in `start()` "for tidiness" → id reuse across cycles breaks the late-reply fence.
- Treating `Changed` payload as state → forbidden; it may be used only as an expected-lineage fence for the next fetch.
- Fixing Tara's mock by adding `ExportAllSlots` → the reply types would still be `v`; the correct mock is the real resident (oracle B).
- Keeping `QindaQt::DisplayTopology` PUBLIC "in case" → unused installed link dependency; use it or drop it.

## 9. Exact acceptance commands

Build outside the worktree symlink (Pavel's finding): `BUILD=/mnt/d/QindaQt/builds/display-d3-kimi-nyra`.

```sh
# Configure/build (Debug strict), mirroring the dev preset without the symlinked binaryDir
cmake -S . -B "$BUILD/dev" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=ON -DQINDAQT_ENABLE_STRICT_WARNINGS=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF
cmake --build "$BUILD/dev" --target qindaqt_display_client \
  qindaqt_display_client_lineage_tests qindaqt_display_client_publication_tests \
  qindaqt_display_client_operations_tests qindaqt_display_client_coordinator_tests \
  qindaqt_display_client_private_bus_tests
# Focused rows, isolated from the host session
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS -u XDG_RUNTIME_DIR \
  ctest --test-dir "$BUILD/dev" --output-on-failure -R '^qindaqt\.display-client-'
# Direct QtTest totals (record "Totals: N passed, 0 failed" per binary)
for t in "$BUILD"/dev/tests/services/display_client/qindaqt_display_client_*_tests; do "$t"; done
# Display regression (D0–D2 rows must stay green: expect the existing five service rows plus D1 rows)
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS -u XDG_RUNTIME_DIR \
  ctest --test-dir "$BUILD/dev" --output-on-failure -R '^qindaqt\.display-'
# Release strict
cmake -S . -B "$BUILD/release" -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=ON -DQINDAQT_ENABLE_STRICT_WARNINGS=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF
cmake --build "$BUILD/release" && env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS -u XDG_RUNTIME_DIR \
  ctest --test-dir "$BUILD/release" --output-on-failure -R '^qindaqt\.display-client-'
# Installed public-header consumer
DESTDIR="$BUILD/stage" cmake --install "$BUILD/dev" && \
  printf '#include <qindaqt/services/display_client/client.h>\n#include <qindaqt/services/display_client/display_coordinator.h>\n#include <qindaqt/services/display_client/display_transport.h>\n#include <qindaqt/services/display_client/qt_display_transport.h>\nint main(){return 0;}\n' > "$BUILD/consumer.cpp" && \
  c++ -std=c++20 -fsyntax-only -I"$BUILD/stage/usr/local/include" $(pkg-config --cflags Qt6Core Qt6DBus) "$BUILD/consumer.cpp"
# Gates
python3 tools/check-source-shape
python3 tools/validate-docs
/home/cabewse/venv/bin/mkdocs build --strict -d "$BUILD/site"
git diff --check 146fc48358c2659436dec4fc6b6062d23c5ee746..HEAD && git diff --check
git status --porcelain   # expect empty; .omc/ must be absent from the commit
# Post-test residue (expect nothing)
pgrep -af 'dbus-daemon.*qindaqt-display-client'; ls -d /tmp/qindaqt-display-client-private-bus-* 2>/dev/null
```

Adjust the installed include prefix to the configured `CMAKE_INSTALL_PREFIX`; the point is that all four headers compile against the staged tree with only Qt on the include path.

## 10. Decisions I am asking for (real ambiguity only)

1. **Activation (F4):** add `StartServiceByName` to the D3 transport, or name the session component that starts Display1. Everything else in this handoff is implementable without an answer, but the private-bus "no owner" row depends on it.
2. **Announced-epoch fence (§4, optional):** adopt or explicitly decline; if declined, document that epoch replays through one owner are accepted by design and fenced only at `stage()`.

## 11. Status

- Handoff complete; profile set to `finished`, not live. I remain available through this thread for re-inspection of any cited line if Pavel posts a file list; I will not run builds or edit product/test paths.
