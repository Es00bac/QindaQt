# Helena March — Midpoint: blocking findings against the D3 partial (read-only inspection)

- Time: 2026-08-28T20:13:43Z
- Inspected: `/home/cabewse/work_SPaC3/container-wm-workers/display-d3-kimi-nyra` as on disk at this time (Pavel's resume-claim tree: `client.h`, `display_client.cpp` 563 lines, `display_coordinator.h/.cpp`, `qt_display_transport.cpp`, Tara's five tests). Static analysis only; no build.
- Full handoff with contracts, repair order, invariants, negative controls, test rows, and acceptance commands follows in this thread. These are the items Pavel should not build past.

## P0 — must repair before any candidate

1. **Unvalidated wire payloads are published.** `qt_display_transport.cpp:171,177` and `:223,229` use `QDBusPendingReply<T>::value()` (raw `operator>>`) instead of `Display::decodeSnapshotArgument` / `decodeOperationResultArgument` (`display_dbus.h:25-30`, the documented adapter boundary). `display_client.cpp:349` trusts `wireValid`, which the raw decoder clears only on list-cap overflow (`display_dbus.cpp:37-47,266-274`); text/numeric bounds, fingerprint length, protocol version, single-primary are never checked. The audio client precedent validates client-side (`audio_client.cpp:293,471`). Fix in both places: transport decodes via the safe wrappers; Client re-runs `validateSnapshot` / `validateOperationResult` so a hostile fake transport is fenced too.

2. **Transport emits completions synchronously**, violating `display_transport.h:14-15`: `qt_display_transport.cpp:160-163` and `:211-214`. `Client::start()` (`display_client.cpp:99`) therefore fetches with an empty owner, gets an inline failure, flickers `Starting→Unavailable` and arms a 2 s poll (`:464`) that never touches the bus; `Coordinator::begin()` (`display_coordinator.cpp:71`) can see `Unavailable` re-entrantly before it publishes `Staging`. Fix: queue those emissions (`QMetaObject::invokeMethod(..., Qt::QueuedConnection)`).

3. **"No owner" is never reported after initial resolution.** `setOwner` (`qt_display_transport.cpp:129-132`) short-circuits on equality, so an errored/empty `GetNameOwner` reply (`:114`) emits nothing when the owner was already empty. Today the client reaches `Unavailable` only by the accident in (2). **Trap:** guarding `requestSnapshot()` on an empty owner without fixing this leaves a client started with no Display1 running in `Starting` forever. Fix: transport emits `ownerChanged(QString{})` unconditionally after initial resolution; `Client::acceptOwner` must not early-return on an empty owner (`display_client.cpp:265-267`) and must publish `Unavailable`; then `requestSnapshot`/`scheduleRefetch` may refuse to run with an empty owner.

## P1 — repair in the same candidate

4. **No activation.** The transport only calls unique names; nothing ever `StartServiceByName`s the activatable `org.qindaqt.Display1`. Precedent: `qt_settings_transport.cpp:305-322`, `settings_client.cpp:409-419`. Decide: add activation (recommended) or document which component starts the service.
5. **Reply payload lineage is recorded but never checked** (`client.h:84-90`, `display_client.cpp:368-406`); owner echo ignored (`:322,:371`). Complete as `Uncertain("lineage-mismatch")` when owner/kind/transactionId/initiating lineage differ (audio precedent `audio_client.cpp:471-479`).
6. **Exactly-once breaks on `stop(); start();`** — `start()` clears the queue (`display_client.cpp:90`) that `stop()` just filled with the promised `client-stopped` result (`:121-129`). Stop clearing in `start()`; deliver already-final results.
7. **`Coordinator::cancel()` in `Confirming` reports `confirmed=false` for a possibly committed transaction** (`display_coordinator.cpp:87-98,163-169`; `Client::cancel` supersedes the in-flight Confirm at `display_client.cpp:251-253`). Make cancel invalid in `Confirming`; replace `bool confirmed` with a closed `Confirmed/Reverted/Uncertain` outcome.
8. **Coordinator ignores client loss in `AwaitingConfirmation`** (`:100-119`) and runs its own 15 s revert timer from the Preview *reply*, which starts earlier than the server's window (server starts at `AwaitingConfirmation` after apply+observe) — a second timer authority ADR-0016 says consumers must not own. Project `TransactionSummary` from `snapshotChanged` instead; keep any client timer as a rescue that fires after the server window.

## Tara — tests as written cannot prove the named behaviors

- Will not compile under `-Werror`: `tst_display_client_owner_lineage.cpp:83,92,100,108` use `.errorCode` (member is `error`); unused params/vars at `:75-77,:88,:96,:104,:153,:163`; unused `requestId` in `tst_display_client_operation_state.cpp:94,:125,:157`.
- `MockDisplayService1` exports no D-Bus methods (`registerObject(path, this)` default exports adaptors only), uses lower-case method names and a `changed` signal on the wrong interface, so the client never reaches `Ready` in any row; test 1 never re-registers after `unregisterService()` (`:177-186`).
- The other four files register no service; their assertions are either/or or `pending || records>0` tautologies (`tst_display_client_operation_state.cpp:65,98,128,160`; `tst_display_client_service_state.cpp:53-60,93-99,175-176`), and "late reply ignored" has no late reply to ignore.
- Coordinator has zero tests.
- Recommended oracles (detailed in the handoff): a deterministic fake `DisplayTransport` (the injection seam Nyra designed — not a duplicate implementation) for lineage/late-reply/exactly-once/hostile-payload rows, plus one private-bus row composing the real `ResidentDisplayService` with `FakeInventorySource`/`FakeTransactionPort` exactly as `tests/services/display_service/tst_resident_display_service_private_bus.cpp:92-107`.

Pavel: repair (1)–(3) first; they change the transport/client seam that everything else, including Tara's rows, depends on. Note `display_client.cpp` is at 493 non-blank lines — the 500-line review trigger will fire; plan the split now.
