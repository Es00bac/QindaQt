# Aquinas the 2nd → Victor Shaw — Appearance model/page failure diagnosis

- Timestamp: 2026-08-28T14:09:25Z
- State: diagnostic handoff; Victor retains implementation ownership
- Source observed twice: live dirty repair over `ef19a9b`
- Acceptance claim: none

Victor, the preserved `build/dev/Testing/Temporary/LastTest.log:55-72` and
`:550-567` reproduce two independent chains. Your live edits already fix the
first-baseline draft seed (`appearance_settings_model.cpp:431-446`), correct
the invalid-snapshot test back to its local transport
(`tst_appearance_settings_model.cpp:503-520`), and reset the member client/logs
in `init()` (`:190-195`). Keep those changes.

## Model/client chain

The original zero-snapshot/null-model crash came from driving a local
`SequenceTransport` while the model's member `SettingsClient` remained bound to
`m_transport`. The baseline helper then timed out and returned null, and its
callers immediately dereferenced the pointer. The current helper and six
callers are at `tst_appearance_settings_model.cpp:164-179`, `:234`, `:267`,
`:326`, `:393`, `:429`, and `:469`. Two hazards remain:

1. `establishBaseline()` / `answerRefresh()` (`:117-137`) use `QTRY_*` inside
   nested `void` helpers. A timeout returns only from the helper; `makeModel()`
   can later return null without a precise assertion. Make these helpers return
   `bool`, assert every stage at the caller, and `QVERIFY(model != nullptr)`
   before every dereference.
2. Every model is parented to the test object, so old models survive across
   slots and remain connected even though `init()` stops the shared client.
   Prefer a per-test owner with construction order transport → client → model
   and reverse destruction, or explicitly destroy/reset the prior model before
   stopping the client and clearing logs. A fresh unique owner is harmless but
   should not substitute for deterministic fixture teardown;
   `SettingsClient::stop()` clears owner state at
   `src/services/settings_client/src/settings_client.cpp:133-153`.

There is also a production ordering race. `SettingsClient::handleCommit()`
emits `commitFinished`, publishes `Authenticating`, and only then calls
`refresh()` (`settings_client.cpp:329-357`). The model intentionally preserves
an applied sequence/final-snapshot wait in
`appearance_settings_model.cpp:357-374`, but
`handleClientState()` unconditionally calls `abortSequence()` on
`Authenticating` (`:280-299`). The reply therefore destroys the queue or
conflict intent before the authoritative snapshot arrives.

Minimal non-vacuous fix: preserve Saving/Conflict through this expected
reply→snapshot transition only while `m_sequenceActive`,
`m_waitingFinalSnapshot`, or `m_conflictIntent` proves it. Do not ignore every
Authenticating transition. Retain the last accepted snapshot owner/epoch and
compare the next snapshot's lineage; if authority changes in the gap, abort the
queue, retain the draft, and require explicit re-Apply. Never replay the next
key to replacement authority. Unavailable, Degraded, and uncertain commits
should continue to abort. A confirmed rejection has no pending sequence, so it
should stay non-editable during automatic rebaseline, preserve its confirmed
error, and become Ready only after that snapshot; the current test around
`:469-500` should answer the refresh before expecting Ready.

## QML page chain

`appearance_qml_composition.cpp:19-31` calls `create()` immediately after
`setData()`. The generated Tokens plugin can leave the component in
`QQmlComponent::Loading`, which is exactly the preserved “Component is not
ready” followed by `qFatal`/SIGABRT. Reuse the repository's bounded
status-change wait in `tests/controls/control_test_support.cpp:23-51` inside
the production `ensureTokenFacade()` path, then create only from Ready and
return a specific timeout/error. This production seam also protects
`src/apps/settings_center/main.cpp:111-133`; fixing only the test would mask the
real startup race.

The page fixture declares the view before the model
(`tst_appearance_page.cpp:150-159`), so reverse destruction deletes the model
while the view/QML may still bind to it. Declare the model first or explicitly
tear down the view before the model. Report scene errors with `QVERIFY2` rather
than `qFatal` so a failure is diagnostic rather than a core-dump abort.

## Regression assertions

- First snapshot differs from defaults yet ends Ready with draft equal to
  confirmed, `draftDirty == false`, and Apply unavailable.
- Every test drives the exact transport bound to its client, leaves no prior
  model connections, and asserts helper/model success before dereference.
- After Applied but before refresh: client may be Authenticating, model stays
  Saving, exactly one commit exists; matching snapshot alone advances the next
  queued key, and the final matching snapshot alone becomes Ready/clean.
- Conflict remains Conflict before refresh; same-lineage refresh retains the
  draft and sends no next commit.
- Owner/epoch replacement in the reply-to-snapshot gap never replays a queued
  write, retains a dirty draft, and requires explicit re-Apply.
- Token component reaches Ready before singleton lookup; page cases no longer
  SIGABRT, and the view is destroyed before the stub model.

I did not edit product/Git, compile, run tests, or launch any UI/session/input.
The test lines above are Victor's preserved run evidence, not a new acceptance
claim.
