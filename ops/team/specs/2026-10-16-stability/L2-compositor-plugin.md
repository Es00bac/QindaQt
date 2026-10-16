# L2 — Compositor plugin efficiency and window model (container-wm) — Opus

Base: container-wm hub `main` at the commit named in the manager's assignment. Branch `stability/l2-compositor-20261016`.
Worktree: `~/work_SPaC3/container-wm.worktrees/stab-l2-compositor-20261016`. Voice seat: `claude-helper-two`.

## Why
- **Idle CPU.** With the owner idle, the compositor main thread uses 33–42% of a core and the shell main thread
  29–44%. Terminal titles animated by Claude Code / Codex spinners (about 10 Hz each) drive a full re-sync
  10–14 times a second:
  - WindowsChanged is broadcast, though nothing in production subscribes to it.
  - `synchronizeChrome` re-plans every container, and `setRenderPlan` repaints and re-uploads a full-frame chrome
    texture (about 8 MB per tick for the maximized QQTerm container).
  - Task facts re-sample and JSON-encode twice, and blocking GetConnectionCredentials calls run on the main thread.
  - The shell then calls TaskListSnapshot, which re-samples again and re-checks credentials.
- **Snapshot fragility.** One bad window voids the whole shell visibility snapshot: the dock goes Degraded,
  auto-hide stops, and window actions are rejected as stale.
- **Stranded windows.** A window was left at the unplugged HDMI output's coordinates.
- **Stale state.** Dissolved containers leave state behind, logging "no valid maximize area" 350 times.
- Findings items 6, 9, 13 (compositor-plugin parts) and 21.

## Owned paths
- `src/compositor/**` — the QindaQt compositor plugin (`src/compositor/kwin`) and `src/compositor/src`
- `src/hybrid/**`
- `src/shell_visibility/**`, `src/shell_visibility_protocol/**`, `src/shell_visibility_client/**`
- the tests and wiki pages for these

## Outcomes
1. **Caption changes take a cheap path.**
   - A title-only change must not re-plan containers or re-render chrome. When titles must be drawn, repaint
     only the tab strip region.
   - Coalesce caption updates per window to at most 4 Hz, with a trailing flush.
   - `setRenderPlan` compares plans and skips identical ones.
   - `synchronizeChrome` runs only when topology or geometry actually changed.
2. **Snapshot traffic.**
   - Emit `WindowsChanged` and `TaskListSnapshotChanged` coalesced to at most 4 Hz, plus a final flush.
     Do not remove public signals; rate-limit them.
   - Serve `TaskListSnapshot` from a cached, already-encoded document that is invalidated on change. Drop the
     decode-after-encode.
   - Cache caller credentials per unique bus name and invalidate them on NameOwnerChanged. No blocking
     credential calls per request on the main thread.
3. **Visibility snapshot degrades per window.**
   - An off-output window or an "active while hidden" window is excluded or clamped, with a log line, instead
     of invalidating the batch.
   - Task facts must not depend on visibility-snapshot validity.
   - Never activate a hidden tab member; activate its container's visible member instead.
4. **Output removal rescue.**
   - After outputs change, any managed window whose frame lies entirely outside every remaining output is moved
     onto a remaining output: stock KWin evacuates; the fork then re-applies saved frames, which must be
     validated against current outputs.
   - Containers must not keep or re-apply frames from a removed output.
5. **Container bookkeeping.** Every dissolution path forgets container state: close, title drag, context menu,
   chrome drag (`kwinhybridinteraction.cpp` around :294) and keyboard group/tile (`kwinhybridwindowmanagement.cpp`
   around :271 and :350). The cleanest form is one central post-publish diff that forgets vanished container IDs.
6. **No per-sample file or /proc I/O on the main thread** for Wine identity (`wineidentitycache.cpp`). Use a cache
   with explicit invalidation, or move the I/O off the main thread.

## Acceptance
- A deterministic test or benchmark: one window changing its title at 10 Hz for 10 simulated seconds produces at
  most about 40 snapshot builds and emits, zero container re-plans and zero full-frame chrome renders. Report
  before/after counts.
- Visibility snapshot tests: a stranded window, an active hidden member and a mixed valid batch.
- An output-removal rescue test: a nested or virtual scenario if the testing harness supports it, otherwise a
  model-level test, with the reason stated.
- A container-forget test for each dissolution path.
- A focused build of the plugin and its ctests.
- Update the wiki pages for task list, shell visibility and hybrid containers, and add an ADR if signal
  semantics change. Run strict mkdocs and the link checker.

## Reference designs (study, don't depend)
- KDE libtaskmanager: per-row `dataChanged` for title changes and no model resets.
- KWin core: caption change handling and output-removal window evacuation.
- GNOME Shell's window tracker: title updates through property notifications.
