# Shannon the 2nd — replacement-baseline identity-handover blocker

- **Timestamp:** 2026-08-28T17:25:55Z
- **Exact candidate:** `ebc2a2a6713d0d8a6ea61298c483aa6fc77604cb`
- **Tree:** `de15bcb8e9ef7b0f098fa398c31d4411b568bd7e`
- **Severity:** P1 integration blocker
- **Routed repair owner:** Rhea Solis

The repaired epoch fence prevents stale traffic, but membership reconciliation
cannot converge a valid replacement population when an item's stable identity
moves to a different exact key. This can occur when one still-live source moves
its StatusNotifier item to a new object path between watcher baselines (and the
same defect applies to a new key under another owner that legitimately replaces
an unseen old registration).

## Exact reproduction

Against the exact candidate library:

1. Epoch 1: begin owner `:1.10`, register identity
   `org.example.same-identity` at `/old`, and complete population.
2. Epoch 2: report the same identity at `/new` under the same current owner
   generation; do not report `/old`, because the replacement population no
   longer contains it.
3. Complete epoch 2.

Observed direct compiled-probe output:

```text
registration=5 completion=0 count=0 old=0 new=0
probe_exit=1
```

`5` is `RegistryStatus::DuplicateIdentity`. At
`status_notifier_registry.cpp:157-163`, the unseen epoch-1 `/old` item still
owns the reverse identity index while `/new` is admitted, so the valid current-
epoch key is rejected. Completion at lines 45-66 then prunes `/old`, leaving no
item at all. The result contradicts the documented full membership rebaseline
and can visibly erase a provider that is present in the replacement watcher
population.

The current reconciliation fixture only re-observes the same exact keys in its
"full" case (`status_notifier_registry_test_support.h:191-199`). Its duplicate
case updates an already-existing exact key and therefore does not exercise
identity handover (`:201-209`). All registered tests pass despite the defect.

## Required bounded repair

Make epoch reconciliation atomic with respect to old unseen identity claims:
admit or stage a valid current-epoch key whose identity is held only by an old,
not-yet-observed key, without ever publishing two claims. Completion must
produce the exact replacement membership. Preserve ordinary same-epoch
duplicate rejection and last-known-good behavior for malformed/duplicate
updates. Add direct same-owner path handover and cross-owner handover tests,
including event-order permutations and rollback/fail-closed behavior if the old
key is later re-observed.

Fresh exact evidence already passing: strict dependency-light configure; serial
20/20 focused build; exact CTest 3/3; direct QtTest values 17/17, registry
23/23, presentation 9/9; and adjacent applet-runtime, visibility-client,
Display-service-model, and session-lock model rows 5/5. These passing suites do
not cover the reproduced transition. Candidate/Git state remains untouched;
the probe and build live only under `/tmp`.
