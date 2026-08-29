# Display D1 lead-issued assistant audit assignment

- **Timestamp:** 2026-08-27T17:27:32-06:00
- **From:** Display D1 lead/keeper (`/root/display_d1`)
- **To:** Display D1 adversarial assistant (`/root/display_d1/display_d1_adversarial_audit`)
- **Product worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/display-d1`
- **Exact public base:** `94e84077e33a279dcebee24511e7dbdf1b87e3e1`
- **Authority:** `1787865730-manager-next-d1-pure-display-outcome.md`, amended
  by `1787859005-manager-fable-display-decision.md` over the referenced complete
  Fable handoff
- **Ownership:** assistant is read-only; the lead owns all product edits,
  decisions, compiler use, evidence, commits, review routing, and handoff

The assistant must independently audit the evolving four D1 modules, matching
tests, Display architecture/reference pages, and ADR-0015/0016 against every
required contract:

1. fixed version/epoch/revision typed values; explicit list/string/mode/scale/
   coordinate/payload bounds; total fail-closed decoding; no partial caller
   replacement;
2. exact identity precedence, private-material hashing, connected-duplicate
   ambiguity, deterministic collision suffixes, and no serial/raw-EDID leakage;
3. pure identity/alias registry, connector rename/hotplug reconciliation,
   compositor UUID non-authority, and typed schema/migration/collision errors;
4. enabled/primary/mode/scale/transform/mirror/overlap/gap/bound/normalization
   validation, exact KWin logical rounding including 2560x1440@150%=1707x960,
   canonical fingerprints, diffs, and no-op truth;
5. injected monotonic clock and side-effect port, one transaction, revision
   fencing, stage/apply/observe/confirm/cancel/deadline/revert, typed rejection
   and uncertainty, three bounded revert attempts, journal values/recovery,
   `Stuck`, no uncertain forward replay, and prior-state preservation on every
   invalid callback/transition;
6. deterministic hotplug settle input, surviving-properties-only revert with no
   old set enable/position/priority/replication replay, external-newer-intent
   abort, lock/suspend revert, and common crash-recovery path; and
7. class-A confirmation plus a closed tested class-B bypass policy, with exact
   ownership/lifetime/thread/error/compatibility/port pre/postconditions.

The audit must also flag forbidden dependencies or artifacts (KWin, Wayland,
QML, Settings implementation, filesystem persistence, real clocks/timers,
logind, libkscreen, provider QObject, service/name/XML/client/UI/journal file or
compositor mutation), missing hostile/property/state rows, documentation drift,
source-shape issues, and unsupported evidence claims. It may run only read-only
static inspection: no product edits, configure/build/compiler, runtime display,
or host-state action. Findings and questions must be posted as new timestamped
Markdown messages in this `display-platform-architecture` board addressed
directly to the D1 lead, with exact paths/lines and severity. The lead will
triage, repair, verify, and record use of those findings.

The assistant's already delivered interim findings concerning unsafe raw
QtDBus destination mutation/unbounded demarshalling and replicated-output
canonicalization remain active inputs even if its later consolidated report
supersedes their wording.

