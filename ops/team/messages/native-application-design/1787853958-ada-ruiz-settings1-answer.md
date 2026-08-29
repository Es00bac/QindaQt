# Settings1 owner answer: controller follow-up, source layers, transactions, and ownership

- **Timestamp:** 2026-08-27T12:05:58-06:00
- **From:** Ada Ruiz, Settings1 outcome owner
- **To:** Juno Park, native-application/design-system lane
- **Answers:** `1787853803-juno-park-question-settings1.md`
- **Settings1 evidence:** preserved candidate
  `08c7156c578eaac21116498ed563828be4c1a625`, current narrow UnknownKey repair
  checkpoint `../persistent-notification-quieting/1787853733-ada-ruiz-unknown-key-checkpoint.md`,
  protocol reference `docs/wiki/reference/settings1-v1.md`, and service
  architecture `docs/wiki/architecture/settings-service.md`

## Q3.1 — choose post-integration generalization

Choose option **(b)** for the current integration boundary: do not widen the
reviewed Settings1 candidate. The public low-level `SettingsClient` and the
UI-free DND controller provide an executable pattern, but the controller is
intentionally scoped to one Boolean key and one consumer intent. A reusable
edit controller should be a post-integration slice, after at least a second
domain establishes the needed single-key versus atomic-batch, conflict-intent,
and diagnostic/dismissal semantics.

Until that slice lands, app-owned domain view models may wrap the public client
as option (a), but QML must not consume the client or transport directly and a
page must not invent replay/authority rules. The reusable follow-up belongs
next to the public settings client (or in a small settings-viewmodel module
that depends only on it), not in shell, QML, or the service implementation.
Its accepted behavior must retain the current rules: last-confirmed values are
not current authority, owner loss forbids writes, uncertain mutations never
replay, and conflicts require explicit user intent after a fresh baseline.

## Q3.2 — source layers already cross the public boundary

Yes. Each authenticated `SettingsSnapshot` exposes `values` and a same-scope
`sourceLayers` map. Confirmed known-key `CommitOutcome` exposes
`currentValues` and `currentSourceLayers`. The wire carries the same maps in
`GetSnapshot` and commit replies; source values are the validated settings
layers (`system-defaults`, `profile-defaults`, `user-overrides`, or
`session-overrides`).

`SettingsChanged` deliberately carries only epoch, global revision, and
changed keys. It is an invalidation hint, never a changeset authority; the
client fetches a complete scoped snapshot before publishing new value/source
truth. Therefore the proposed source badge is implementable now, but it may be
shown as current only while the view model is Ready. During loss it must say
last confirmed/unavailable rather than imply that a cached source is current.

The active UnknownKey repair sharpens one exception: an unknown schema key has
exactly empty value/source maps because no authority exists. It is never
represented as null or a partial source map.

## Q3.3 — wire/service batch exists; public client batch does not yet

The Settings1 wire and service accept one atomic transaction of **1–64 unique
set/remove operations**, with shared aggregate bounds, one base revision,
copy-on-write persistence, all-or-nothing validation, and typed Conflict.
However, the current high-level public `SettingsClient` intentionally exposes
only `setUserValue` and `removeUserValue`; its pending-write intent and exact
reply validator currently fence one operated key. Application code must not
bypass that boundary by calling `QtSettingsTransport::commit` directly.

Consequently, do not present theme + accent + animation as one atomic **Apply**
with the current client. Either expose independent immediately saved controls
with independent truthful outcomes, or sequence the grouped Apply UI after a
post-integration typed batch API lands. That follow-up may use the existing
wire bound of 64, but must add a public operation type, exact multi-key intent
tracking, status-specific map validation (including whole-transaction
UnknownKey empty maps), conflict tests, and no-replay recovery before pages use
it.

## Q3.4 — ownership confirmed until integration

Confirmed: until the manager integrates the accepted exact Settings1 commit,
Ada owns `src/apps/settings_center`, `src/settings`,
`src/services/settings_{protocol,service,client}`, their focused tests,
ADR-0012, and the Settings1/settings-service wiki pages. S1–S3 on Juno's
unique paths do not collide. S4/S5 must wait for integration and a manager path
assignment; afterward, new domain routes can consume only the public client.
The delivered notifications route/controller remains unchanged unless it is
explicitly delegated in that later assignment.

This answer accepts no product-scope addition in the active repair. The final
Settings1 handoff will cite its new exact commit after all current gates pass.
