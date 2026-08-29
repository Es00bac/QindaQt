# Shannon exact rereview — interrupted-initial-baseline P1 blocker

- **Timestamp:** 2026-08-28T18:07:05Z
- **Reviewer:** Shannon the 2nd
- **Exact rejected candidate:** `4144303f0506e0f33a1ffd29feb952825a9e4d2d`
- **Tree:** `5096acc0130d2bafcb086815bda08a2fdd10276f`
- **Sole parent:** `ebc2a2a6713d0d8a6ea61298c483aa6fc77604cb`
- **Severity:** P1; one atomic-reconciliation root cause, with identity and
  capacity manifestations

## Reproduction

A fresh ignored standalone C++ probe compiled the candidate registry and
validation sources. It first began watcher epoch 1, admitted a partial
population, deliberately did **not** complete epoch 1, then began replacement
epoch 2 and reported a valid complete target.

For a same-owner stable identity moving from `/old` to `/new`:

```text
interrupted_identity registration=5 completion=0 count=0 old=0 new=0
```

`5` is `DuplicateIdentity`. The valid target registration is rejected against
the abandoned epoch-1 identity claim; completion returns Accepted, prunes the
old key, and publishes neither key.

For an abandoned 64-item partial population followed by a valid 64-item target
of 63 retained exact keys plus one unique replacement:

```text
interrupted_capacity registration=6 completion=0 count=63 replacement=0
```

`6` is `CapacityExceeded`. The valid replacement is rejected against abandoned
epoch-1 capacity; completion returns Accepted and publishes only 63 of 64.

The complete probe exits 1 with four failed convergence assertions:

```text
identity=1 capacity_first=1 capacity_last=1 conflict_old_first=1 conflict_new_first=1 invalid_capacity=1 interrupted_identity=0 interrupted_capacity=0 failures=4
compile_exit=0 probe_exit=1
```

Thus the completed-LKG repair paths are genuinely green, but the
interrupted-first-baseline transition remains broken.

## Root cause and repair contract

`StatusNotifierRegistry::beginWatcherEpoch()` at
`src/shell/status_notifier/src/status_notifier_registry.cpp:44-52` selects
staging solely with `m_hasCompletedPopulation`. When epoch 1 has admitted a
partial population but has not completed, epoch 2 therefore stays in direct
admission mode while the abandoned `m_items` and `m_identityOwners` remain.
Identity uniqueness and `kMaxItems` are evaluated against the wrong epoch; the
completion-time prune happens too late to recover the rejected target item.

Rhea must produce one non-amended descendant that makes every replacement
watcher target epoch-local even when no prior population completed. Either
stage the incoming target independently or safely discard the never-accepted
partial snapshot before direct admission; do not expose, validate, or capacity-
count current-epoch items against abandoned membership. Add registered
regressions for interrupted initial partial identity handover and 64→64
replacement (new-first and new-last where meaningful), assert the exact target
after completion, stale-epoch fencing, Loading truth before completion, and no
regression to completed-LKG rollback.

Candidate/product bytes and Git state were never edited. The detached review
worktree remains clean. Do not integrate `4144303f`; return Rhea's exact clean
descendant to Shannon for immediate rereview.
