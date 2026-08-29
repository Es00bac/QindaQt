# Manager decision on the Fable display/output analysis

- Time: 2026-08-27 13:30 MDT
- Analyst result:
  `display-platform-architecture/1787858968-elara-finch-fable-analysis-handoff.md`
- Analyst base: `dc29c88911f0ed6d381211027f16f46bbf92a07c`
- Current integrated base: `c4982697858c083828bd406f1aa56c4e942bcc10`
- Decision: accept the architecture boundary with the amendments below. This
  is routing authority, not implementation or qualification evidence.

## Accepted boundaries

1. KWin remains the sole live and restore authority. QindaQt must never read,
   write, or infer authority from `kwinoutputconfig.json`.
2. One focused, D-Bus-activated `org.qindaqt.Display1` service is QindaQt's
   production writer to the pinned KDE output-management protocol. The shell,
   Settings Center, Color, and brightness consumers cross through typed public
   clients rather than raw protocol or service internals.
3. Display1 owns preview, confirmation deadline, revert journal, restart
   recovery, and topology reconciliation. A UI, shell overlay, or compositor
   plugin must not own the authoritative timer.
4. The direct protocol adapter is the production integration. libkscreen and
   `kscreen-doctor` are test/oracle dependencies only unless a later ADR
   supersedes this decision.
5. Compositor changes are additive and bounded: a revisioned output inventory,
   a development-gated virtual-hotplug seam that is rejected in production,
   and a later rollback-safe Hybrid group-reflow collaborator.
6. Shell geometry continues to derive from the compositor/Qt inventory path
   and never waits for Display1. Settings1 owns bounded policy/registry values,
   not live topology.
7. Every handoff must separate deterministic unit/private-bus/nested evidence
   from physical DRM/GPU/monitor/lid/suspend qualification.
8. KWin's immediate persistence of a successful preview is the hardest D2
   invariant. Hotplug/restart recovery must prove convergence to the surviving
   pre-image without reading KWin's store or fighting KWin's own re-query.

## Required amendments

- ADR allocation is now: 0013 QST-1, 0014 Audio1, 0015 Display1 transaction
  authority, 0016 persistent output identity, 0017 pinned KDE output protocol,
  and 0018 the ADR-0007 selector/fractional-metadata amendment.
- The analysis predates integrated Settings1. New display policy keys require
  a bounded schema-v3 migration owned by the Settings module; Display workers
  must not edit around the public Settings1 boundary.
- Display1 must consume the existing authenticated, fail-closed public
  session-lock client. It must not create a second direct or unauthenticated
  `org.freedesktop.ScreenSaver` monitor. Unavailable or inconclusive lock
  authority denies preview and reverts an active preview.
- The suspected Qt fractional-scale mismatch is a hypothesis, not an accepted
  code change. A measurement-only nested row at 1.25 and 1.5 comes first.
  Dropping DPR equality is allowed only if evidence proves the mismatch and
  the replacement retains an exact name/geometry/generation bijection.
- The proposed 10 MiB Display1 PSS ceiling is not accepted as a fixed budget
  before a representative Qt/Wayland baseline exists. D8 must measure idle
  PSS, private dirty memory, wakeups, and transaction peaks, then record an
  evidence-based ceiling. The no-periodic-wakeup requirement is accepted.
- Plasma KScreen OSD/backend coexistence is a packaging decision. No worker may
  mask, stop, or modify the host user's Plasma services while developing or
  qualifying QindaQt.
- Class-B brightness/color fields remain provisional until their owning
  platform lanes confirm that the KWin configuration transport preserves
  device error truth and does not collapse DDC-CI, ICC, or ambient-policy
  authority into Display1.

## Authorized slice order

1. **M0 measurement:** existing nested 1.25/1.5 QScreen, Compositor1, and shell
   inventory parity. Read-only with respect to host desktop state.
2. **D0:** Compositor1 output generation plus development-only add/remove
   virtual-output seam, exact KWin 6.6.5 pin, production rejection.
3. **D1:** pure display protocol/identity/topology/transaction modules with no
   D-Bus, Wayland, QML, or filesystem dependency.
4. **D3b foundation:** typed asynchronous display client on the accepted
   protocol values. Its service-availability extraction waits for the second
   accepted client boundary as already recorded.
5. **D2:** resident service, protocol adapter, journal, recovery, lock client,
   and logind delay-inhibitor integration. D2 cannot claim completion without
   hotplug-during-preview and service-restart recovery.
6. **D3a, D4, D5, D6, D7, D8:** Settings schema, shell overlays/selectors,
   Hybrid reflow, Displays page, policy, then release qualification, each as a
   separately reviewed vertical slice.

M0, D0, and D1 have disjoint primary path ownership and may run in parallel
after the currently accepted Settings/QST/Audio integration boundary is
settled. D2 waits on D0 and D1. D6 waits on the native AppShell/Controls
foundation rather than inventing a standalone visual stack.

## Mandatory first evidence

- Verify the exact runtime model/result and clean analyst worktree (done).
- Measure fractional inventory parity before a shell fix (pending M0).
- Prove dev output mutation is unavailable without the existing pre-parse
  development gate (pending D0).
- Pin and checksum the KDE protocol XML and licensing manifest (pending D1/D2).
- Prove lock-authority loss denies preview, session-bus/Wayland loss exits the
  exact service process, and recovery never leaves an orphan (pending D2).
- Decide packaged-session KScreen OSD/backend coexistence before interpreting
  host-with-Plasma nested runs as QindaQt release behavior (pending D8).

