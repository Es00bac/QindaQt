# Shannon the 2nd — interrupted-baseline Status Tray exact PASS

- **Timestamp:** 2026-08-28T19:10:42Z
- **Verdict:** **PASS**
- **P0/P1/P2/P3:** **0/0/0/0**
- **Exact accepted commit:** `4c26af45d6aae3aea3adb4569e4627a9c3d0a34f`
- **Exact tree:** `37a12c32533b202bfbd6f7a17e5f7a8cec5d5cd3`
- **Exact sole parent:** rejected `4144303f0506e0f33a1ffd29feb952825a9e4d2d`
- **Review worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/system-tray-s0-atomic-rereview-shannon`
- **Candidate cleanliness:** detached `HEAD`; empty final
  `git status --porcelain=v1`

## Exact repair disposition

The interrupted-first-population P1 is closed. `beginWatcherEpoch()` now
distinguishes a real watcher replacement from the first begin. When no
population has ever completed, it clears the abandoned direct epoch's item,
reverse-identity, last-seen, and degradation state before the new epoch admits
anything. The set was never accepted as last-known-good, so clearing it cannot
erase an accepted snapshot. Once any population has completed, the prior
bounded staged target path remains unchanged: matching completion publishes
both target indexes together, while duplicate or over-capacity targets retain
the exact actionable LKG.

A fresh ignored standalone probe linked the freshly strict-built exact library
and exited 0 with:

```text
interrupted_identity registration=0 completion=0 count=1 old=0 new=1 cleared=1 loading=1
interrupted_capacity order=first registration=0 completion=0 count=64 displaced=0 replacement=1 cleared=1 loading=1
interrupted_capacity order=last registration=0 completion=0 count=64 displaced=0 replacement=1 cleared=1 loading=1
conflict order=old-first conflict=5 invalid_completion=5 lkg=1 actionable=1 recovered=1
conflict order=new-first conflict=5 invalid_completion=5 lkg=1 actionable=1 recovered=1
completed_capacity order=first registration=0 completion=0 lkg_before=1 exact=1
completed_capacity order=last registration=0 completion=0 lkg_before=1 exact=1
invalid_capacity registration=6 invalid_completion=6 lkg=1 actionable=1 recovered=1
failures=0
```

`0` is Accepted, `5` DuplicateIdentity, and `6` CapacityExceeded. This directly
replays both former poison manifestations and independently preserves the prior
replacement orders, conflict orders, invalid completion, last-known-good
actionability, stale-epoch/Loading truth, exact membership, and next-epoch
recovery contracts. The registered public atomic rows also pass 4/4 and include
the completed same-owner and cross-owner identity handovers plus malformed-new-
item behavior.

## Independent verification

- Fresh dependency-light Debug configure: Ninja, GCC 16.1.1, Qt 6.11.1;
  repository strict warnings and `CMAKE_COMPILE_WARNING_AS_ERROR=ON`; KWin,
  shell, production shell, and host-uinput disabled. The worktree `build`
  symlink resolves exactly beneath `/mnt/d`.
- Fresh serial focused build of the library and three test executables:
  **20/20** actions, exit 0.
- Exact focused discovery and run:
  `^qindaqt\.status-notifier-(values|registry|presentation)$`: exactly **3**
  rows, **3/3 PASS**.
- Complete direct QtTests: values **17/17**, registry **25/25**, presentation
  **9/9**.
- Named hostile direct subsets: registry **7/7**, values **6/6**, presentation
  **4/4**. The registry subset contains epoch reconciliation, both atomic
  public rows, malformed LKG retention, and generation/epoch exhaustion.
- Fresh adjacent serial build: **130/130** actions, exit 0. Exact adjacent
  selector for applet runtime resolution, ShellVisibility client, Display
  service model, and session-lock authentication/transitions discovers exactly
  five rows and passes **5/5**.
- `PYTHONDONTWRITEBYTECODE=1 python3 tools/check-source-shape
  --warnings-as-errors`: **1019** source files, **0** skips; registry source
  **490** and registry suite **494** nonblank lines. The atomic helper remains
  separately decomposed.
- `PYTHONDONTWRITEBYTECODE=1 python3 tools/validate-docs`: **65** Markdown
  documents plus navigation PASS. This dependency-light registry has zero
  separate `docs|links` CTest rows, so an empty selector was not used as
  evidence.
- MkDocs 1.6.1 `build --strict --site-dir build/mkdocs-site`: PASS, with all
  output kept under the required ignored build symlink.
- Manual diff/API/test review covered interrupted repeated replacement, exact
  identity and capacity indexes, stale registration/completion, Loading before
  completion, degradation clearing, completed-LKG staging and rollback,
  request actionability, counter exhaustion, modularity, test non-vacuity, and
  the owning tray wiki/ADR/harness claims. No new public API or dependency was
  introduced and no additional finding remains.
- `git diff --check HEAD^..HEAD` and `git show --check`: PASS. Provenance is
  exact: one parent-to-candidate commit and exactly five changed paths. Local
  current `main` `c4982697858c083828bd406f1aa56c4e942bcc10` is the exact merge
  base and an ancestor (`main...HEAD` = **0 behind / 36 ahead**); classic
  merge-tree inspection contains no conflict markers. Final detached candidate
  status is byte-clean.

## Review-environment note and boundary

The required `/mnt/d` build symlink initially caused Qt AUTOMOC to canonicalize
generated-output paths and emit relative includes toward a nonexistent sibling
source directory. Those attempts stopped on generated MOC includes before
candidate product compilation at the affected targets. The reviewer applied
only CMake's documented symlink-safe `AUTOMOC_PATH_PREFIX` plus current-
directory include settings in ignored build metadata, cleaned all outputs, and
then obtained the fresh exact 20-action and 130-action passes above. No source,
test, documentation, candidate Git, or host state was changed by that build-
environment correction.

This remains deliberately source/unit evidence with a fake transport. It does
not claim live QtDBus watcher binding, item property decoding, DBusMenu revision
transport, a rendered panel tray, or assistive-technology bridge behavior. No
host bus, tray, GUI, desktop/session, compositor/display, input, configuration,
hardware, or user data was contacted.

## Manager action

Integrate exact commit `4c26af45d6aae3aea3adb4569e4627a9c3d0a34f` and rerun
the affected focused gates on the conflict-resolved manager tree. Do not amend
or substitute another tree under this verdict. Rhea's preserved writer
worktree may be retired only after this exact commit is preserved by
integration. Shannon the 2nd is finished and releases the runtime slot while
remaining the permanent Status Tray reviewer for any later manager-routed
milestone.
