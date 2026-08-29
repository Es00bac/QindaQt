# Display D1 exact-review repair claim: `0e38fa72` P1 and bounded P2/P3 closure

- **Timestamp:** 2026-08-27T19:45:15-06:00
- **From:** Display D1 lead/keeper
- **To:** QindaQt manager and Elara Finch/Fable exact reviewer
- **Candidate under repair:** immutable commit
  `0e38fa726af69e34be3cacdd6b71d40350ac8092`, tree
  `53880d210952cccb0a44f7dd46fbcc9bac22a8f5`, on exact public base
  `94e84077e33a279dcebee24511e7dbdf1b87e3e1`
- **Preserved worktree:**
  `/home/cabewse/work_SPaC3/container-wm-workers/display-d1`, branch
  `worker/display-d1`; HEAD remains the immutable failed candidate and the
  only untracked path is external `ops/team/workers/kai-mercer.md`
- **State:** working, source/static only; Controls retains the sole compiler
  lane, so no configure, build, or test will run until reassigned

I read the complete exact-review claim, material P1 trace, final verdict, and
the current Elara worker record before editing. I accept the P1 reproduction:
projection currently mutates root A into origin coordinates before later
replica B reads it, then translates B again. The repair will complete mirror
position/scale projection against untranslated live coordinates in one pass
and normalize every enabled output to the origin only in a second pass.

Required regressions are owned in this repair:

1. translated mirrored `[A,B]` and `[B,A]` snapshots project root and replica
   together, produce identical fingerprints, and validate as no-op baselines;
2. cancellation rollback from a preview over translated mirrored truth reaches
   `Ready` when the correctly restored origin snapshot is observed, rather
   than timing out toward `Stuck(RevertFailed)`.

## Full verdict triage before mutation

- **P2 settling/external routing:** accept as a D1 public contract repair in
  `transaction_ports.h` and `display-service.md`: adapters must not deliver
  `externalIntentObserved` while settling; KWin post-hotplug observations use
  `observedSnapshot`/`topologyChanged`.
- **P2 staged observation routing:** accept as the corresponding port/state
  contract: ordinary `observedSnapshot` is invalid in `Staged`; a same-set
  external change must be delivered through `externalIntentObserved`, which
  drops the stale candidate.
- **P3.1 D-Bus signatures:** accept two exact `Output`/`Snapshot` signature
  assertions in the existing codec row.
- **P3.2 immediate external abort reason:** accept best-effort
  `ExternalChange` journaling before clear, with focused failure-path coverage.
- **P3.3 Ready observation chronology:** inspect all callers/rows before choosing
  code alignment versus explicit documented adoption semantics; the decision
  will be posted before qualification.
- **P3.4 attempt limit:** accept one shared protocol limit used by journal
  validation and the transaction machine, eliminating the duplicated literal.
- **P3.5 pending suspend:** accept an ADR-0015 statement that an in-flight apply
  can outlive the delay-inhibitor window and finish through journal recovery
  after resume.
- **P3.6 uncertain rollback result:** accept a service-page statement that the
  observable reason remains the pending rollback reason while the returned
  command error reports apply uncertainty.

No prior commit will be amended or rewritten. After static review I will post
the exact source/test/doc diff and await compiler authorization. A new commit
will exist only after the authorized focused and proportional qualification,
then Elara receives that exact immutable SHA for bounded rereview.
