# Talia North — exact repaired-candidate regression review complete

## Decision

- **Exact candidate:** `55105b2c565f25f0582303e4936bcd288b04ffdb`
- **Preserved rejected predecessor:** `00b3d49ac3d7ba94edcf10272fa5e61185d63b56`
- **Review checkout:** detached, clean, and still at the exact candidate
- **Verdict:** **REJECT / BLOCKED FROM INTEGRATION**
- **Finding set:** complete for this exact candidate and assigned repair matrix
- **Remaining findings:** **2 P1 repair classes; no additional P2**

The repaired commit substantively fixes every one of Rowan Ivers's original six
P1 blockers and two P2 issues. It cannot be accepted because fresh end-to-end
review reproduced two additional P1 classes at public Settings1/UI boundaries:
the DND projection can present stale or erased outcome truth, and the generic
Object contract can crash on null or silently corrupt accepted integers across
persistence. A new exact repaired commit is required.

## P1 blockers, severity ranked

1. **The DND controller can remain in stale Saving/Conflict across authority
   loss and erases a confirmed persistence-failure diagnostic after refresh.**
   `DoNotDisturbController::handleClientState()` ignores every client transition
   while its presentation state is Saving or Conflict at
   `src/services/settings_client/src/do_not_disturb_controller.cpp:79-83`.
   Owner loss after an Applied reply but before its refresh therefore leaves
   “Saving…” indefinitely; owner loss in Conflict leaves an unusable Apply
   action rather than honest Unavailable/Retry. Confirmed non-Applied outcomes
   are assigned at `do_not_disturb_controller.cpp:127-148`, but the automatic
   refresh at `settings_client.cpp:351-356` drives the next snapshot through
   `do_not_disturb_controller.cpp:99-124` and clears the diagnostic. A private,
   ignored-build lifecycle probe reproduced all three stable results:
   `saving=1 unavailable=0`, `conflict=1 unavailable=0 apply=0`, and
   `ready=1 error=''` after PersistenceFailed plus successful refresh. Full
   finding and repair evidence: `1787849070-talia-north-repair-review-finding.md`.

2. **Schema-valid generic Object values do not survive the advertised wire and
   save/restart contract.** `decodeJsonValue()` accepts null and unrestricted
   unsigned integers at
   `src/services/settings_protocol/src/settings_wire_decode.cpp:248-260`, while
   the Object normalizer shallow-passes the map at
   `src/settings/src/settings_value_normalizer.cpp:106-112` and persistence
   delegates it to `QJsonObject::fromVariantMap()` at
   `src/settings/src/settings_document.cpp:94-110`. On a private D-Bus, committing
   a nested invalid/null QVariant aborted the sender inside libdbus with exit
   134 because a D-Bus variant has no untyped-null payload. A separate run
   successfully committed `uint(4000000000)` and `qulonglong(UINT64_MAX)`, then
   observed the latter written as `18446744073709552000`; after service
   reconstruction the types had drifted to `qlonglong`/`double` and the wide
   integer no longer survived. Full finding and executable evidence:
   `1787849341-talia-north-object-roundtrip-finding.md`.

## Original rejection matrix disposition

1. **Opaque recursive-value and reply-envelope bounds — repaired.** The shared
   aggregate budget charges nodes/bytes during streaming decode before child
   retention in `settings_wire_decode.cpp:37-78,343-362`; private-D-Bus hostile
   coverage at `tst_settings_protocol_dbus.cpp:69-136` exercises node, byte, and
   envelope overflow. Snapshot/commit validators enforce exact outer fields at
   `settings_reply_validation.cpp:81-92,146-203`.
2. **Serialized activation/backoff/no-owner completion — repaired.** Transport
   and client maintain explicit in-flight state at
   `qt_settings_transport.cpp:269-309` and `settings_client.cpp:388-459`.
   `tst_settings_client.cpp:216-283` proves failure backoff, no-owner completion,
   duplicate-terminal suppression, and later recovery; the adversarial private-
   bus test also observes one terminal failure per attempt.
3. **Owner/epoch/revision lineage — repaired.** The transport captures owner
   generation in relays and replies at `qt_settings_transport.cpp:38-113,161-177`.
   Commit validation requires exact epoch/schema/base/status/revision/maps/
   changed-key relationships at `settings_reply_validation.cpp:111-203`.
   `tst_settings_client.cpp:285-484` covers same-owner epoch contradiction,
   contradictory commit replies, bounded invalidations, and target-loop
   rejection; `tst_qt_settings_transport_adversarial.cpp:91-181` fences late
   replies and signals from the prior generation.
4. **Profile-default authority and remove fallback — repaired.** Production
   requires and composes the installed profile document before user overrides
   at `resident_settings_service.cpp:92-129`; migrated user data is persisted
   only after name acquisition at `:131-157`. Private-bus lifecycle coverage at
   `tst_settings_service_lifecycle.cpp:29-144` proves the QindaQt `160`
   profile-default value and fallback after removing a user override; `:147-266`
   rejects incompatible profile/user documents and exercises migration.
5. **Initial/repeated start failure and recovery — repaired.** Client/controller
   coverage at `tst_settings_client.cpp:182-215,216-283` proves honest
   Unavailable/Retry, repeated synchronous failure, bounded diagnostics, and a
   later Ready transition. Both focused offscreen surfaces passed their current
   failure/retry scenarios. This disposition does not clear the distinct
   post-commit controller races in P1 finding 1.
6. **Independent application/two-shell/service reconstruction persistence —
   repaired.** `tst_notificationquietingsettingsbridge.cpp:100-241` commits
   through the ordinary controller, constructs and reconstructs shell bridges,
   reconstructs the service from one isolated file, and baselines a fresh shell
   bridge; revision assertions prove consumers did not replay state.
7. **Same-object transport restart — repaired.** Teardown clears exact-owner and
   local-bus subscriptions and generation state; real-transport coverage in
   `tst_qt_settings_transport.cpp:22-154` exercises start/ready/stop/start/ready
   on the same object.
8. **Focused regex/docs — repaired.** The normative regex at
   `docs/wiki/development/testing-harness.md:329` includes
   `notification-quieting-controls-offscreen`, and strict docs/link gates pass.

Ada's follow-up global-revision fix also holds: every valid bounded
invalidation refreshes the repository-global commit base even when its keys are
outside a scoped client's requested set; the client invalidation cases at
`tst_settings_client.cpp:467-484` pass. No further P1/P2 was confirmed in the
assigned exact-commit epoch/base/status/revision/map checks, repeated startup
recovery, staged profile contract, or restart/reconstruction paths beyond the
two blockers above.

## Independent verification on the exact hash

- Strict Debug configuration with shared libraries, tests, production shell,
  KWin plugin disabled, and host-uinput tests disabled: **exit 0**.
- Built the settings protocol/service/client, quieting bridge, private-D-Bus
  adversarial, lifecycle, and offscreen targets: **exit 0**.
- Focused C++ registry:
  `^qindaqt\.(settings-(protocol|protocol-dbus|service-lifecycle|client|qt-transport|qt-transport-adversarial)|notification-quieting-settings-bridge)$`
  — **7/7 passed**.
- Focused offscreen registry:
  `^qindaqt\.(settings-app-offscreen|notification-quieting-controls-offscreen|notification-surfaces-offscreen)$`
  — **3/3 passed**.
- `qindaqt-settings_qmllint` — **exit 0**.
- `tools/check-source-shape --largest 30` — **exit 0**, **760 files**, zero
  violations; largest repaired production sources remain below 500 nonblank
  lines (SettingsClient 490, wire decoder 482).
- `tools/validate-docs` — **exit 0**, **42 documents**.
- `mkdocs build --strict` into the review build directory — **exit 0**.
- `git diff --check 00b3d49..55105b2` — **exit 0**.
- Two ignored-build controller/Object probes used only isolated private D-Bus,
  temporary files, and offscreen/in-process objects; their reproduced outcomes
  are recorded in the findings above.
- Final `git status --short --branch`: `## HEAD (no branch)`; final HEAD exactly
  `55105b2c565f25f0582303e4936bcd288b04ffdb`.

This reviewer did not rerun Ada's full Debug/Release 68-test registries,
production staged install, or activation-descriptor smoke test; those remain
implementer evidence. No live desktop, user session bus, compositor, input,
pointer, keyboard, lock service, KGlobalAccel, uinput, or accessibility bridge
was accessed.

## Required next action

Do not integrate `55105b2`. Repair both P1 classes in Ada Ruiz's isolated
worktree with controller race/diagnostic tests and exact-metatype private-bus
Object save/restart tests, then publish one new exact commit. A different worker
must re-review that exact repaired hash; passing prose or tests on any other
tree cannot clear this rejection.
