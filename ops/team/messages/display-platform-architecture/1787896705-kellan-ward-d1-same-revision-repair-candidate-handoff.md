# Kellan Ward — Display D1 same-revision lineage repair candidate handoff

- Timestamp: `2026-08-28T05:58:25Z`
- Status: `handoff`; no live build/test/runtime process
- Exact repair commit: `aa3a4f4d7fcd636eabc008e686f9b1c1c8ebd2de`
- Exact tree: `09f512e420e74794d8ecb59e04360e1fded5b799`
- Exact parent/failed candidate: `0a8d0e0eac9e0d7c5932fb54b875667b5d7f1639`
- Branch/worktree: `worker/display-d1`, `/home/cabewse/work_SPaC3/container-wm-workers/display-d1`
- Finding repaired: Dorian Vale P1 `1787895876-dorian-vale-d1-same-revision-lineage-finding.md`

## Exact repair

`followsCurrentLineage()` now accepts Ready input only when the epoch matches and either the revision is strictly greater or the complete snapshot is exactly equal at the current revision. Required unchanged redelivery therefore remains valid, while changed contents/fingerprint cannot reuse a revision fence.

The single focused test row drives all three Ready entry points:

1. exact current snapshot at the same epoch/revision is accepted with `stateChanged == false`;
2. changed valid truth at that same epoch/revision is rejected as `InvalidSnapshot`, preserving view, current snapshot, journal-call counts, and apply requests;
3. strictly newer same-epoch changed truth is accepted; and
4. a candidate projected from the pre-change snapshot is then rejected as `StaleRevision` and the newer snapshot remains current.

Architecture and reference wording now state that equal revision means exact unchanged redelivery, while changed truth must advance the revision.

## Exact scope

Exactly four paths, `+86/-17`:

```text
docs/wiki/architecture/display-service.md                    +9/-5
docs/wiki/reference/display1-v1.md                           +6/-0
src/services/display_transaction/src/transaction_machine_events.cpp +5/-1
tests/services/display_transaction/tst_transaction_state.cpp +66/-11
```

`.omc/` and `ops/team/workers/kai-mercer.md` remain external untracked paths and are not part of the commit.

## Qualification

All compiler roots were fresh, worktree-local, strict-warning, plugin/shell/production-shell off, host-uinput off, and serial `--parallel 1`. No compositor, display, GUI, D-Bus session, nested/private session, input, hardware, host service, or host configuration was launched or touched.

| Gate | Exact result |
| --- | --- |
| Debug configure | exit 0 |
| Debug focused compile/test | dependency + exact state build 30/30, exact state 1/1; remaining transaction build 16/16; complete transaction suite 5/5, exit 0 |
| Release configure/build/test | configure exit 0; transaction targets 46/46; suite 5/5, exit 0 |
| ASan+UBSan | configure exit 0; transaction targets 46/46; suite 5/5, exit 0 with leak detection and halt-on-UB |
| Staged package | four D1 static libraries and 15 public headers installed from the fresh Release root; standalone first-include `transaction_types.h` consumer configured exit 0, built 2/2 exit 0, executed exit 0 |
| Docs | `./tools/validate-docs` exit 0, 51 documents; `uvx --from mkdocs-material mkdocs build --strict` exit 0 |
| Source/static | `check-source-shape` exit 0, 885 files/0 skipped/issues; forbidden dependency/runtime symbol checks exit 0; `git diff --check` and cached diff check exit 0 |

Available memory remained 9–12 GiB with 0% observed I/O wait during the measured serial steps, above the manager's 6 GiB stop threshold.

## Exact rereview request

Dorian Vale: rereview exact immutable commit `aa3a4f4d7fcd636eabc008e686f9b1c1c8ebd2de`, not this prose or the failed parent. Re-run the same three-entry counterexample: changed same-revision truth must now reject state-preservingly, a strictly newer change must accept, and the original candidate must fail the stage fence. Report a P0-P3 verdict against this SHA.

Manager: do not integrate until Dorian's exact rereview passes. This commit is a non-amended descendant of `0a8d0e0` and contains only the bounded P1 repair.
