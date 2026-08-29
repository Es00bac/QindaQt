# Rowan Ivers — exact-candidate adversarial review complete

## Decision

- Candidate: `00b3d49ac3d7ba94edcf10272fa5e61185d63b56`
- Base: `496e5135ee4f40359f8b871eec130f0b8b02a241`
- Review checkout: detached and clean at the exact candidate
- Verdict: **REJECT / BLOCKED FROM INTEGRATION**
- Finding set: **complete for this exact candidate**
- Findings: **6 P1 integration blockers, 2 P2 repair issues**

Passing build and test evidence does not override the bounded-wire, activation,
lineage, profile-authority, startup-recovery, and required reconstruction-proof
failures below. The implementer must provide a new exact repaired commit; prose
or tests run against a different tree cannot clear this verdict.

## P1 blockers

1. **Opaque D-Bus values are recursively materialized before aggregate bounds.**
   The protocol decoder enforces depth and per-container counts while expanding
   opaque `QDBusArgument`, but only applies the promised aggregate byte/node
   budgets after materialization. The Qt transport similarly `qdbus_cast`s a
   top-level reply map before bounded decoding. This leaves a same-session
   resource-exhaustion path and lacks real opaque/private-bus adversarial proof.
   Full finding: `1787802504-rowan-ivers-finding.md`.

2. **Activation failure retries are immediate and unbounded.**
   An unavailable owner plus repeated `StartServiceByName` failure recursively
   launches a new asynchronous activation request without delay or an in-flight
   guard; explicit refresh can stack more attempts. This contradicts the
   documented bounded retry behavior and can consume CPU/bus capacity.
   Full finding: `1787802579-rowan-ivers-finding.md`.

3. **Commit replies and invalidations are not fenced to accepted lineage.**
   Commit handling does not match epoch/base revision or enforce status/revision
   invariants and bounded exact result fields. Invalidation keys are unbounded,
   a far-future revision can force perpetual refresh, and transport delivery can
   relabel a queued old subscription callback with the current owner. The
   promised `(owner, epoch, revision)` authority is therefore incomplete.
   Full finding: `1787802625-rowan-ivers-finding.md`.

4. **The resident Settings1 authority omits profile defaults.**
   Production startup creates schema/system defaults and applies only the user
   override document; it never loads the shipped profile-default document.
   A private-bus `GetSnapshot` for `appearance.animationDurationMs` returned
   `180` / `system-defaults`, while the shipped QindaQt profile specifies `160`.
   This violates the documented four-layer generic authority.
   Full finding: `1787835655-rowan-ivers-finding.md`.

5. **Synchronous transport startup failure strands both UIs in Loading.**
   `SettingsClient::start()` can return false without publishing a transition;
   the DND controller stays in its initial Loading state, production roots only
   log the error, Retry is hidden, and controller refresh cannot restart a
   client that never started. This violates honest Unavailable/error state and
   explicit recovery requirements.
   Full finding: `1787835656-rowan-ivers-finding.md`.

6. **The required persistence proof across shell reconstruction is absent.**
   Existing real-transport coverage keeps clients alive across service
   replacement, while bridge coverage injects owner changes into the same
   client/controller/bridge. No executable scenario commits, destroys and
   reconstructs the shell consumer, reconstructs the service from the same
   isolated file, and baselines a fresh shell consumer. The explicit task and
   ADR consequence are therefore claimed without their required proof.
   Full finding: `1787835894-rowan-ivers-finding.md`.

## P2 repair issues

1. **A `QtSettingsTransport` instance cannot restart after `stop()`.**
   Teardown sets `started=false` before its guarded owner-unbind and never
   disconnects the local-bus `Disconnected` subscription. A private-bus
   start-ready-stop-start probe produced:
   `firstStarted=1 firstReady=1 secondStarted=0 secondReady=0`, with
   `cannot observe local session bus disconnect`. Shipped roots reconstruct
   objects, so this is medium rather than P1. Full finding:
   `1787836110-rowan-ivers-finding.md`.

2. **The documented focused gate omits the purpose-built shell-control test.**
   The testing-harness regex does not select
   `qindaqt.notification-quieting-controls-offscreen`, although the following
   prose claims the state/focus/accessibility evidence it supplies. The omitted
   test passes when called explicitly. Full finding:
   `1787836175-rowan-ivers-finding.md`.

## Verification performed on the exact candidate

- Configured a separate strict Debug build with shared libraries, tests,
  production shell, no KWin plugin, and host-uinput tests disabled: **exit 0**.
- Built the settings model/protocol/service/client, settings application,
  production shell, and focused test targets: **exit 0**.
- Ran the focused Settings1, settings-app, bridge, and quieting-control set:
  **13/13 passed, 0 failed**.
- Ran `qindaqt-settings_qmllint`: **exit 0**.
- Ran `tools/check-source-shape --largest 30`: **exit 0**, 758 files checked,
  no violation; the largest new production settings client source was 419
  non-blank lines.
- Ran `tools/validate-docs`: **exit 0**, 42 documents.
- Ran `mkdocs build --strict` into an isolated site directory: **exit 0**.
- Ran `git diff --check 496e513..00b3d49`: **exit 0**.
- Ran two isolated private-session-bus probes: one reproduced the missing
  profile layer; one reproduced the transport restart failure.
- Final review checkout `git status --short`: **clean**.

Rowan did not rerun the candidate handoff's Release/full-66-test/staged-install
commands; those claims remain the handoff author's evidence. This review did
independently inspect activation/install metadata and found no separate layout
blocker. No live compositor, real session bus, lock service, KGlobalAccel,
uinput, pointer, keyboard, or desktop automation was used.

## Other audited areas

No additional independent blocker was found in the immutable-v1/active-v2
schema split, explicit v1-to-v2 migration codec, repository copy-on-write save
ordering, revision-exhaustion handling, DND-versus-lock privacy precedence,
fixed notification-settings launcher boundary, ordinary settings-app linkage,
activation descriptor/install layout, source modularity, or documentation/link
shape beyond the findings above.

## Required next action

Do not integrate `00b3d49`. Repair all six P1 findings and the two bounded P2
issues in the implementer's isolated worktree, add the missing hostile/private-
bus/reconstruction/restart/UI recovery coverage, then publish one new exact
candidate SHA with focused and broad evidence. Review must restart from that
exact repaired commit and explicitly recheck every numbered finding.
