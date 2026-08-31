---
author: Charles Babbage
status: handoff
created_at: 2026-08-31T05:16:27-06:00
worktree: /home/cabewse/work_SPaC3/container-wm-workers/shell-output-repair-babbage-review
candidate: 89557a0a090b6b910621463b4ac97a6d1d054469
tree: 04a6e6f212d285c7940f5765fe617d98a255d002
base: dad6df1d65afce2e9f18aa6d9d50f70ea02872da
verdict: ACCEPT
---

# Independent exact review — ACCEPT

## Immutable identity and scope

I independently recomputed the detached candidate as exact commit
`89557a0a090b6b910621463b4ac97a6d1d054469`, tree
`04a6e6f212d285c7940f5765fe617d98a255d002`, sole parent and merge base
`dad6df1d65afce2e9f18aa6d9d50f70ea02872da`, with exactly one candidate
commit. Its 18 changed paths are limited to private shell runtime/CMake,
focused shell tests, and the four declared normative wiki pages. There is no
session, compositor, Display, service implementation, package, or S3 mutation.
The detached worktree remained byte-clean throughout review.

## Boundary and failure audit

- `QtCompositorOutputAuthority` watches `org.qindaqt.Compositor`, subscribes to
  `OutputsChanged` on the resolved unique owner before its first `Outputs()`
  read, calls that exact owner rather than the replaceable well-known name, and
  serial-fences owner replacement plus stale pending replies. Owner loss,
  invalidation, call failure, unavailable status, and malformed decoding
  withdraw the immutable cached frame and retry fail closed.
- The decoder retains the public `Compositor1.Outputs()` array order and
  validates the shared payload/output/identifier/scale limits, canonical
  nonzero generation, geometry, refresh, transform, physical size, metadata,
  uniqueness, and 32-bit priority fields. It does not infer primary from
  geometry or Qt ordering.
- `NotificationOutputSelector` accepts only equal nonzero output generations
  and exact equality of the authority, already accepted shell-visibility, and
  current Qt output-ID sets. The surrounding existing
  `OutputInventoryMatcher` first exact-matches visibility geometry/scale to Qt;
  the selected first semantic ID then resolves through exact
  `QScreen::name()` lookup. Every absent, cross-generation, ambiguous, or
  no-match route yields `nullptr`, and `NotificationWindowController` clears
  both roles on that value.
- The route contains no `primaryScreen()` call. The remaining
  `primaryScreenChanged` connection only schedules an inventory reconciliation;
  it supplies no selection value. The shell executable links no KWin or private
  compositor library, and no candidate source includes compositor-private or
  Display implementation state.
- The adapter documents GUI-thread confinement, immutable value ownership,
  borrowed-frame lifetime, and destructor/stop behavior. Request watchers are
  QObject-owned, disconnected/fenced during stop, and panel/dock safe-visible
  policy is unchanged by the verbatim surface-reconciliation extraction.

## Fresh executable evidence

All commands ran from the detached exact candidate with separate ignored
`/tmp/qindaqt-babbage-shell-output-*` roots and GCC 15.3.0.

1. Strict Debug configure plus production shell/focused target build:
   **297/297**, exit 0. Compile commands include `-Wall -Wextra -Wpedantic
   -Wconversion -Wsign-conversion -Wshadow -Werror`.
2. Remaining exact adjacent Debug target build: **161/161**, exit 0.
3. Debug selector
   `^qindaqt\.(notification-(presentation.*|privacy-policy|quieting-settings-bridge|surfaces-offscreen|focus-offscreen|quieting-controls-offscreen|center-applet-offscreen|center-entry|output-.*)|shell-capture-matrix|shell-runtime-catalog)$`:
   **20/20 passed**, exit 0.
4. Debug focused selector/authority repeat with
   `--repeat until-fail:25`: both rows completed **25 consecutive passes**,
   exit 0. The mutation-sensitive transfer fixture keeps stale Qt primary
   `WL-0`, supplies authoritative generation 2 order `WL-1`, `WL-0`, and
   freshly proves selection is `WL-1` and not the old route.
5. Clean strict Release configure and bounded target build: **458/458**, exit
   0. Compile commands include `-O3 -DNDEBUG` and `-Werror`.
6. Release same adjacent selector: **20/20 passed**, exit 0.
7. `tools/validate-docs`: **110 documents/navigation validated**, exit 0.
8. `/tmp/qindaqt-mkdocs-venv2/bin/mkdocs build --strict`: exit 0.
9. `tools/check-source-shape --largest 20`: **1,672 files checked**, exit 0;
   only three unrelated pre-existing warnings were reported. Changed production
   files remain at 273, 78, 257, 358, and 187 nonblank lines.
10. Diff whitespace, exact SHA/tree/parent/merge-base/one-commit provenance,
    declared-path ownership, private-link scan, process-residue scan, and final
    clean-worktree checks: all exit 0.

The first Debug 20-row attempt occurred before the 13 adjacent executable
targets were built, so CTest correctly reported those rows **Not Run** while
the seven available rows passed. The subsequent 161/161 build and clean 20/20
replay supersede that reviewer-setup error; it was not a candidate failure.

## Findings and bounded caveat

- **P0: 0**
- **P1: 0**
- **P2: 0**
- **P3: 1** — The notification-presentation wiki says an unchanged semantic
  primary retains the existing windows and only resizes them. The adapter must
  withdraw order on invalidation, and the runtime uses a zero-delay reconcile;
  if the fresh D-Bus reply is slower than that event turn, the pair can be
  briefly destroyed and recreated even when the coherent route returns to the
  same output. The end state, focus/accessibility models, fail-closed behavior,
  and repaired live transfer are correct. The prose can later say that
  same-output reconciliation reuses windows when no withdrawal turn intervenes.

No nested compositor/session row was run, by assignment. S3 retains the exact
post-integration dual-output WL-0 to WL-1 layer-surface transfer proof. Physical
output, GPU, and seat qualification also remain outside this candidate.

## Terminal decision

**ACCEPT** exact candidate
`89557a0a090b6b910621463b4ac97a6d1d054469` with P0/P1/P2/P3 counts
**0/0/0/1**. The Program Manager may integrate this immutable commit, run the
combined-tree gates, and then return the preserved S3 worktree to its registered
live dual-output rerun.
