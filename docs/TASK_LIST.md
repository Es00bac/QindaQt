# QindaQt product task list

This is the outcome-oriented source of truth for active product work. It does
not count assignments, processes, reviews, or partially implemented code as
completion. Architectural detail and long-range milestone state remain in the
[implementation roadmap](wiki/development/implementation-roadmap.md).

## Active outcome: persistent notification quieting

A user can control Do Not Disturb from a first-class QindaQt settings surface,
and the committed choice survives a complete settings-service and shell
restart without weakening lock-screen privacy.

Completion requires all of the following on the integrated branch:

1. A generic, bounded `org.qindaqt.Settings1` service owns user persistence,
   nonwrapping revision order, copy-on-write transactions, migration, and
   change publication through the validated settings model. No client reads or
   writes settings JSON directly.
2. The schema has an explicit Do Not Disturb key with migration/default
   behavior. Existing valid settings continue to load, and corrupt, stale, or
   unsupported data fails without partial mutation.
3. The production shell initializes and updates its injected interruption
   policy through an owner-authenticated asynchronous Settings1 client. Service
   loss has documented deterministic behavior and cannot override the
   authenticated lock-state privacy gate.
4. The ordinary `qindaqt-settings --page notifications` route and the shell's
   Settings1-backed quick control expose keyboard/screen-reader semantics,
   pending/conflict/unavailable truth, explicit recovery, and no direct
   dependency from the app to shell presentation internals.
5. The setting survives save/reopen and independent service/shell restart
   tests. Transactions prove no-op, conflict, rollback, malformed input, owner
   replacement, timeout, and transport-loss behavior.
6. Focused Debug and Release tests, the complete QindaQt registry, production
   build, QML lint, source-shape check, strict documentation build, and staged
   installation pass. Live desktop interaction is reported separately and is
   never implied by offscreen or private-D-Bus evidence.
7. The owning wiki pages and an ADR describe service authority, persistence,
   migration, client authentication, failure policy, and UI boundaries.

## Completed outcomes

- `11c1f4b` — Notification presentation is denied unless an authenticated
  compositor-bound lock monitor conclusively reports `Unlocked`; transport
  loss revokes visibility immediately.
- `c93c45e` — Session-scoped Do Not Disturb policy and presentation behavior.
- Hybrid interaction, Compositor MVP, and Foundation are complete as recorded
  in the implementation roadmap.

## Later outcomes

- Audio, power, brightness, Bluetooth, network, clipboard, display, color,
  font, portal, and policy platform services.
- The complete applet-based settings center and remaining first-party desktop
  experiences.
- Physical hardware, performance/memory, packaging, recovery, migration, and
  upgrade qualification.
