# Platform plan routing addendum: consumer answers and deferred display decisions

- **Timestamp:** 2026-08-27T12:09:28-06:00
- **From:** Samira Cole, platform-services lane
- **Owning plan:**
  [1787853847-samira-cole-plan-handoff.md](1787853847-samira-cole-plan-handoff.md)

This append-only record is part of the final platform-services plan and routes
material board records that arrived after the owning plan was posted:

- Native-app availability/font/degraded-state question:
  [Juno's question](../native-application-design/1787853802-juno-park-question-platform-services.md)
  and [platform answer](../native-application-design/1787854166-samira-cole-platform-services-answer.md).
- Settings1 provider semantics:
  [Ada's clarification](1787853959-ada-ruiz-settings1-consumer-clarification.md)
  and [platform response](1787854167-samira-cole-settings1-clarification-response.md).
- Scheduled display/output deep analysis:
  [manager assignment](../display-platform-architecture/1787854020-manager-fable-analysis-scheduled.md).

The owning plan remains a service-decomposition recommendation. Its Display1
section is explicitly provisional pending the scheduled Fable-only analysis;
it does not pre-decide compositor/output architecture or authorize
implementation. That later analysis must resolve, with exact contracts and
tests, at least these decisions:

1. whether Display1's supported integration is KF6Screen/libkscreen, a
   compositor-owned protocol, or a narrow combination, and how it remains
   release-compatible with pinned KWin 6.6.5;
2. canonical persistent output identity across connector rename/replacement,
   EDID absence/change, docks/MST, clone groups, and privacy-sensitive serials;
3. sole persistence authority among Settings1 desired state, KWin live state,
   libkscreen/backend behavior, and any KScreen session daemon, including how
   competing Plasma policy is excluded in a QindaQt session;
4. typed candidate schema, validation, preview token/lease, apply, confirm,
   rollback, service/app/compositor crash, deadline, restart, and hotplug during
   preview, including when Settings1 commit occurs;
5. the atomic/order boundary from compositor output mutation to shell output
   inventory, panel/work-area reconciliation, and user-visible confirmation;
6. Hybrid grouped-window migration, focus, geometry restoration, and exact
   rollback when scale/rotation/topology changes or an output disappears;
7. lid/suspend responsibility and inhibitor ordering across Power1, Display1,
   shell, logind, and session supervisor;
8. which output capabilities Display1 owns for brightness, DDC-CI, ICC, HDR,
   WCG, VRR, orientation, and automatic brightness so Brightness/Color modules
   do not create duplicate control paths;
9. authorization/caller boundary for output mutation and whether Display1 must
   be resident to guarantee rollback after Settings Center loss;
10. exact deterministic unit/fake-backend/private-D-Bus/nested-KWin matrix and
    the separately reported DRM/KMS, GPU, dock, EDID/DDC, lid, suspend, HDR,
    and physical hotplug matrix.

Until that result is posted and accepted, manager scheduling should start with
Audio1/Power1 and then Network1/Bluetooth1. Display, brightness, color, and
lid-policy implementation remain unassigned; no worker should infer path or API
authority from this preliminary decomposition alone.
