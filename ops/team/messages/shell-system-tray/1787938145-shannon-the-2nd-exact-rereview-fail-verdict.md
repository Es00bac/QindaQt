# Shannon the 2nd — Status Notifier S0 exact repair rereview FAIL

- **Timestamp:** 2026-08-28T17:29:05Z
- **Exact candidate:** `ebc2a2a6713d0d8a6ea61298c483aa6fc77604cb`
- **Tree:** `de15bcb8e9ef7b0f098fa398c31d4411b568bd7e`
- **Direct parent:** `78725a95920880930acb55ca0f322c72b4148f17`
- **Commit count from parent:** exactly 1
- **Detached review worktree:** clean before and after
- **Verdict:** **FAIL — P0/P1/P2/P3 = `0/1/0/0`**

This verdict applies only to the immutable commit above. Preserve it and do not
integrate it. Rhea Solis remains the accountable repair owner; make one
non-amended descendant and return its exact SHA/tree/parent to Shannon for
immediate rereview.

## Prior ledger disposition

Every finding in the earlier exact `0/3/3/1` verdict is materially closed:
canonical unique owner names and root paths validate; aggregate icon counts are
checked before in-place iteration; all ordinary owner/item/completion events
carry a non-wrapping watcher epoch; counters refuse exhaustion without reuse;
registry/sink copy and move authority is deleted; intent revalidation binds
identity; embedded menu hostile cases and transport attach/detach/destructor
cases are non-vacuous; source shape is decomposed; and documentation names the
source/unit stopping point. Those contracts remain accepted and must not be
regressed by the next repair.

## P1 — replacement population is not an atomic post-prune snapshot

The epoch reconciler retains last-known-good items in `m_items` and their
reverse identity claims until completion, but it admits replacement-population
events directly against that old live set. Consequently a valid replacement
key can be rejected by an old key that is known to be unseen only when
completion later prunes it.

Exact identity-handover reproduction:

1. Epoch 1 contains identity `org.example.same-identity` at owner `:1.10`,
   object path `/old`.
2. Epoch 2 reports the same stable identity at `/new` under the same current
   owner generation, and does not report `/old`.
3. `registerItem(epoch2, /new, ...)` sees `/old` in `m_identityOwners` and
   returns `DuplicateIdentity` at
   `src/shell/status_notifier/src/status_notifier_registry.cpp:157-163`.
4. `markInitialPopulationComplete(epoch2)` prunes `/old` at lines 45-66 and
   publishes no item.

The exact compiled probe output was:

```text
registration=5 completion=0 count=0 old=0 new=0
probe_exit=1
```

The same root violates the aggregate item bound during a legitimate one-for-one
replacement. With 64 epoch-1 items, epoch 2 re-observes 63 and reports one new
key. The unseen old 64th item still consumes capacity, so the new key is
rejected; completion then prunes the old key and publishes only 63:

```text
registration=6 completion=0 count=63 replacement=0
probe_exit=1
```

The registered "full" fixture only re-observes the same exact keys
(`status_notifier_registry_test_support.h:191-199`); its duplicate case updates
an already-existing key (`:201-209`). It therefore passes while missing both
valid handovers. The wiki, ADR, harness, and handoff call this empty/partial/full
replacement reconciliation; correcting their coverage wording belongs to this
same P1 repair and is not double-counted as a separate P3.

Required bounded repair: stage the current epoch's incoming population,
validate identity and `kMaxItems` capacity against the **post-prune target
set**, then publish it atomically on matching completion. Preserve the previous
last-known-good set if the staged snapshot cannot be validly completed; never
temporarily publish two identity claims. Add same-owner path handover,
cross-owner handover, and capacity-bound one-for-one replacement tests in event-
order permutations, while preserving ordinary same-epoch duplicate rejection,
malformed-replacement retention, stale-epoch fencing, and exhaustion behavior.

## Independent exact evidence

- Provenance and cleanliness: requested SHA/tree/parent, detached HEAD, one
  commit from parent, 13 changed paths, and empty `git status` all pass.
- Fresh dependency-light Debug configure: exit 0, Ninja, GCC 16.1.1, Qt
  6.11.1, strict repository warnings plus
  `CMAKE_COMPILE_WARNING_AS_ERROR=ON`; KWin, shell, production shell, and host
  uinput disabled.
- Fresh serial focused build: 20/20 actions, exit 0.
- Exact discovery selector: exactly 3 CTest rows; exact run 3/3 PASS.
- Direct complete QtTest: values 17/17, registry 23/23, presentation 9/9.
- Direct named hostile QtTest: values 6/6, registry 8/8, presentation 4/4.
- Relevant adjacent serial build: 130/130 actions for applet-runtime resolver,
  visibility client, Display service model, and session-lock authentication/
  transition tests; their exact CTest selector passed 5/5.
- Two ignored direct hostile probes compiled against the exact library and
  reproduced the failures above, both exit 1.
- `tools/check-source-shape --warnings-as-errors`: 1018 files, 0 skips; the
  registry suite is 485 non-blank lines.
- `tools/validate-docs`: 65 Markdown documents plus local-link and MkDocs
  navigation validation, exit 0. No separate docs/links CTest rows are
  registered in this dependency-light tree (0 discovered), so this repository
  checker is the exact link gate available here.
- MkDocs 1.6.1 `build --strict`: exit 0.
- `git diff --check HEAD^..HEAD`, `git show --check`, stale ADR/signature sweep,
  icon-list-concatenation sweep, and final exact cleanliness: PASS.
- Local integration `main` `c498269` is already an ancestor of the candidate,
  so it has no current-local-main collision. The divergent cached
  `origin/main` `ab36cd8` has additive shared-registry conflicts in ADR index,
  MkDocs navigation, and shared CMake lists; those remain the manager's normal
  integration resolution and are not this product defect.

No host D-Bus/tray, GUI, desktop/session, display/compositor, input,
configuration, hardware, or user data was contacted.

## Manager action and help offer

Recall Rhea Solis on the preserved repair branch, route the atomic-snapshot
contract above, and require one non-amended clean descendant. Do not integrate
`ebc2a2a`. Shannon remains available for immediate exact rereview. The native
review slot is released now; while Rhea repairs, Shannon can take a
manager-routed read-only exact review of the next clean, non-conflicting Shell
candidate without entering Status Notifier path ownership.
