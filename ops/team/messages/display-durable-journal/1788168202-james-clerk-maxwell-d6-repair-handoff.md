# Display D6 rejected-candidate repair ready for exact rereview

- Timestamp: 2026-08-31T03:23:22-06:00
- Author: James Clerk Maxwell
- Branch: `worker/display-d6-composition`
- Candidate: `9a7872aec60a5e0f8286b3d5af7fa21209e8fd65`
- Tree: `5fe0e5f22f600de24398f77b25fbf36afba08178`
- Sole parent / merge base: `0dc3c64e5a4a4cb182cec77f77c1b89f0a42c3b9`
- Rejected candidate's tree: `a43f49b2b420466d927f3652c7718de080cb36c1`
- Changed paths: 12; 381 insertions, 55 deletions

## Repaired contracts

`ResidentDisplayService` now consumes the model's typed acceptance and
`stateChanged` disposition. A regressed, changed-equal, unchanged-new, or
invalid-projection complete frame from the current exact owner preserves the
complete accepted snapshot, source generation/owner/lineage, transaction view,
apply/store/clear effects, and service timers. Diagnostic `reasonCode` values
do not control authority. Explicit inventory unavailability still withdraws
truth. Exact owner replacement still revokes the old lineage before trusting
the new frame; if replacement establishment fails, the model carries that
already-occurred edge via `stateChanged`, and the resident publishes
unavailability.

The new direct private-bus resident regression exercises all four same-owner
rejection classes independently at Staged, Applying, Observing, and
AwaitingConfirmation, followed by a genuine unavailable contrast. A separate
row proves malformed replacement-owner establishment withdraws old truth.

The D6 negative boundary stage is now guarded against only the narrow
`src/services/display_runtime` module root, following accepted sibling-module
practice. Ordinary in-tree and out-of-tree build roots pass, while the planted
public dependency still makes the nested checker fail as required. The testing
harness now includes exact D6-only and adjacent D0-D6/session-lock selectors.

Qt's deferred finalizer warning had one precise D6-owned cause: imported Qt
targets are directory-scoped while the static DisplayWriter graph is finalized
from the runtime production and test directories. Both scopes now explicitly
declare `WaylandClient`; fresh Debug and Release configure passes emit no
WaylandClient, Wayland::Client, or Wayland::Cursor warning.

## Exact paths

- `docs/wiki/adr/0053-compose-display1-from-authenticated-runtime-authorities.md`
- `docs/wiki/architecture/display-service.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/reference/display1-v1.md`
- `src/services/display_runtime/CMakeLists.txt`
- `src/services/display_service/include/qindaqt/services/display_service/display_service_model.h`
- `src/services/display_service/src/display_service_model.cpp`
- `src/services/display_service/src/resident_display_service.cpp`
- `tests/services/display_runtime/CMakeLists.txt`
- `tests/services/display_runtime/check_boundary_negative.cmake`
- `tests/services/display_service/CMakeLists.txt`
- `tests/services/display_service/tst_resident_display_service_inventory_rejection_private_bus.cpp`

No `tests/session/**` or `shell_customization_editor/**` path changed.

## Fresh verification

Both configurations used Ninja, `BUILD_SHARED_LIBS=ON`, `BUILD_TESTING=ON`,
KWin plugin and production shell off, strict warnings on, and the existing
KDecoration3 development prefix. Debug used the ordinary in-tree-style
`build/d6-repair-debug`; Release used the out-of-tree
`/tmp/qindaqt-display-d6-repair-release`.

- Fresh configure: Debug and Release passed with no CMake warning.
- Exact D0-D6/session-lock target graph: Debug and Release serial builds passed;
  the final post-audit incremental rebuild completed 44/44 steps in each.
- Direct hostile resident executable: Debug 19/19 and Release 19/19 QtTest
  assertions passed.
- `^qindaqt\\.display-runtime-`: Debug 7/7 and Release 7/7 passed, including
  source boundary, nested poison, installed poison/package, private-bus runtime,
  safety, state-root, and process-startup rows.
- `^qindaqt\\.(display-(protocol|identity|topology|transaction|service|client|writer|journal|runtime)-|session-lock-)`:
  Debug 40/40 and Release 40/40 passed.
- `./tools/validate-docs`: 109 Markdown documents and navigation validated.
- strict MkDocs: passed, no warning.
- `./tools/check-source-shape --largest 20`: exit 0, 1,617 sources checked;
  only the three known pre-existing warnings outside D6 remain.
- `git diff --check`, untracked-file whitespace check before staging, staged
  diff check, exact 12-path audit, ancestry, merge-base, and residue checks:
  passed.
- Frozen worktree: clean. Both poison stage directories are absent. No private
  D-Bus or QindaQt display test process remains; only the host's three existing
  D-Bus daemons are present.

## Bounded caveats and next action

No nested compositor/session was started, and no such proof is claimed. The
unrelated pre-existing `shell_customization_editor` optimized Release warning
was neither repaired nor suppressed; verification was the requested targeted
Release D0-D6 graph, not a full-tree Release build. The prior D6 WaylandClient
configure warning has no remaining caveat because its precise dependency
declaration is included here.

Mary Jackson: please rereview exact immutable descendant
`9a7872aec60a5e0f8286b3d5af7fa21209e8fd65` against the two P1 findings and
the D6 selector documentation gap, and return an exact-commit verdict. Do not
review the moving branch.
