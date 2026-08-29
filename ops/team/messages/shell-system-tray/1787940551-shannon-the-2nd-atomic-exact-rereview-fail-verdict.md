# Shannon the 2nd — atomic Status Tray exact rereview FAIL

- **Timestamp:** 2026-08-28T18:09:11Z
- **Verdict:** **FAIL**
- **P0/P1/P2/P3:** **0/1/0/0**
- **Exact rejected commit:** `4144303f0506e0f33a1ffd29feb952825a9e4d2d`
- **Exact tree:** `5096acc0130d2bafcb086815bda08a2fdd10276f`
- **Exact sole parent:** `ebc2a2a6713d0d8a6ea61298c483aa6fc77604cb`
- **Review worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/system-tray-s0-atomic-rereview-shannon`
- **Candidate cleanliness:** detached `HEAD`; empty `git status --porcelain=v1`

## Blocking finding

**P1 — a watcher replacement that interrupts the never-completed initial
population is not epoch-local.** `beginWatcherEpoch()` selects staging with
`m_reconcilingPopulation = m_hasCompletedPopulation` at
`src/shell/status_notifier/src/status_notifier_registry.cpp:44-52`. If watcher
epoch 1 admitted a partial population but never completed, epoch 2 therefore
validates and mutates directly against abandoned epoch-1 `m_items` and reverse
identity claims. Completion prunes the abandoned keys only after it has already
rejected valid current-target registrations.

Fresh ignored exact-source probe output:

```text
interrupted_identity registration=5 completion=0 count=0 old=0 new=0
interrupted_capacity registration=6 completion=0 count=63 replacement=0
identity=1 capacity_first=1 capacity_last=1 conflict_old_first=1 conflict_new_first=1 invalid_capacity=1 interrupted_identity=0 interrupted_capacity=0 failures=4
compile_exit=0 probe_exit=1
```

`5` is `DuplicateIdentity`, `6` is `CapacityExceeded`, and `0` is Accepted.
The current watcher can therefore complete with no item after a valid identity
handover or with 63/64 after a valid capacity-bound target. This is the same
atomic post-prune reconciliation defect class at an interrupted-first-baseline
boundary, not a duplicate independent severity. The exact reproduction and
repair contract are also in `shell-system-tray/1787940430`.

## Passing evidence

- Fresh strict dependency-light configure: exit 0; Ninja; Debug; GCC 16.1.1;
  Qt 6.11.1; repository strict warnings and
  `CMAKE_COMPILE_WARNING_AS_ERROR=ON`; KWin, shell, production shell, and host
  uinput disabled.
- Serial focused build of the library and three tests: **20/20** actions, exit
  0.
- Exact focused discovery/run:
  `ctest -R '^qindaqt\.status-notifier-(values|registry|presentation)$'`:
  exactly **3** rows, **3/3 PASS**.
- Complete direct QtTests: values **17/17**, registry **25/25**, presentation
  **9/9**.
- Named hostile direct subsets: registry **7/7** (atomic identity/capacity,
  empty/partial/full reconciliation, malformed LKG retention, epoch/generation
  exhaustion); values **6/6** (icon, tooltip, menu, canonical unique names);
  presentation **4/4** (watcher-loss LKG and fake attachment/rebaseline/
  reconciliation/destructor detachment).
- The original completed-LKG standalone controls all pass: same-owner identity,
  64→64 replacement-first and replacement-last, both conflict orders with
  rejected completion plus actionable LKG and next-epoch recovery, and invalid
  capacity completion with actionable LKG: **6/6**, `failures=0`, exit 0.
- Fresh adjacent serial build: **130/130** actions. Exact adjacent selector:
  applet runtime resolution, ShellVisibility client, Display service model, and
  session-lock authentication/transitions: exactly **5** rows, **5/5 PASS**.
- `PYTHONDONTWRITEBYTECODE=1 python3 tools/check-source-shape
  --warnings-as-errors`: **1019** files, **0** skips; registry suite **494**
  nonblank lines and new fixture separately decomposed below the threshold.
- `PYTHONDONTWRITEBYTECODE=1 python3 tools/validate-docs`: **65** Markdown
  documents plus navigation PASS.
- `/tmp/qindaqt-docs-venv/bin/mkdocs build --strict`: MkDocs 1.6.1 PASS.
- Manual diff/API review covered epoch/generation exhaustion, watcher arrival
  and replacement, empty/partial/full completed baselines, item/owner/caller
  bounds, canonical owners and paths, service/transport ownership and lifetime,
  destructor detachment, accessibility/localization, test non-vacuity,
  modularity/source shape, and owning wiki/ADR/testing pages. No second finding.
- `git diff --check HEAD^..HEAD` and `git show --check`: PASS. Provenance is
  exact, parent-to-candidate count **1**, changed paths **8**. Local `main` is an
  ancestor (`main...HEAD` = **0 behind / 35 ahead**) and classic merge-tree
  collision scan has no conflict markers. Final candidate status is byte-clean.

## Caveat and manager action

The valid boundary remains source/unit-only with a fake transport: no live
QtDBus watcher, item decoding, DBusMenu revisions, rendered tray, or assistive-
technology bridge is claimed. No host bus, tray, GUI, session, compositor,
display, input, configuration, hardware, or user data was contacted.

**Do not integrate `4144303f`.** Return the preserved writer worktree to Rhea
for one non-amended descendant that makes a replacement target epoch-local even
when the preceding initial epoch never completed. Add registered interrupted-
baseline identity and 64→64 target regressions, including target ordering,
Loading/stale-epoch truth, and completed-LKG rollback non-regression. Then route
the exact clean descendant back to Shannon the 2nd for immediate rereview.

Shannon has read the fresh Shell queue and peer routing, is finished and
releases this runtime slot while remaining the retained reviewer. Compatible
help offer: immediate rereview of Rhea's descendant; if the manager can do so
without delaying that loop, a bounded read-only exact review of another clean
non-conflicting Shell candidate is also available.
