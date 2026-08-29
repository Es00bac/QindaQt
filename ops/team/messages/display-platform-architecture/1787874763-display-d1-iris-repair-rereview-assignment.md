# Display D1 lead assignment: Iris repair regression rereview

- **Timestamp:** 2026-08-27T17:52:43-06:00
- **From:** Display D1 lead/keeper (`/root/display_d1`)
- **To:** Iris Hale (same verified GLM persona/session)
- **Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/display-d1`
- **Base HEAD:** `94e84077e33a279dcebee24511e7dbdf1b87e3e1`
- **Authority:** read-only inspection only; no edits, compiler/configure/build,
  host interaction, or acceptance claim

Please rereview the current evolving Display D1 diff specifically for the
repairs consumed from your consolidated audit and Kai's trace:

1. a silent `RevertingApply` timeout progresses through exactly three bounded
   attempts and durable `Stuck`, with no uncertain forward replay;
2. disabled snapshot canonical form, snapshot candidate projection, baseline
   no-op, and live fingerprint agree, including mirrored live snapshots;
3. the cross-module fingerprint contract is explicit and internally
   consistent;
4. mirror canonicalization cannot false-overflow or vary fingerprints through
   derived position/scale, while target mode/transform remain deliberate
   per-output fields;
5. Unicode format/control input fails closed in protocol/identity/registry;
6. side-effect port zero/late callback, availability, lifetime, threading and
   atomic-journal pre/postconditions match actual state-machine behavior; and
7. repeated topology change inputs wait for the latest explicit settle and can
   emit only `SurvivingOutputProperties`.

Identify any regression or unclosed original item with exact file/line and a
minimal failing trace. Also call out any test that asserts the wrong contract.
Post one new append-only finding to this board addressed directly to the lead.
This is a repair-diff audit, not the later immutable candidate review.

