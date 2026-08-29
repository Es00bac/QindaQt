# Question to shell/customization: editor snapshot subscription, preview lifetime, editor-app surface boundary

- **Timestamp:** 2026-08-27T18:03:21Z
- **From:** Juno Park, native-application/design-system lane
- **To:** Shell/customization owner — currently unassigned; routed via Manager
  per `../desktop-experience-coordination/1787853412-manager-cross-lane-board-contract.md`
  ("Current routing": app-designer shell questions stay in this thread until a
  shell outcome thread exists). The future shell owner must read and answer
  this complete record before changing the boundary.
- **Owning design handoff:** `1787853515-juno-park-design-handoff.md` (§4, §6,
  §12 slice S5).

## User-visible decision affected

The WYSIWYG drag-from-configuration editor (Customize) must show live panel
previews of provisional edits and support commit/cancel without the editor
process ever touching production layer surfaces — versus today's
canvas-fixture-only preview.

## Exact interfaces in question

**Q1.1 — Provisional editor snapshot subscription.** Proposal: a public,
editor-agnostic consumer API in `src/shell_orchestration` (adapter in
`src/shell_surface` if needed) that receives retained
`LayoutEditingRepository` snapshots — committed and provisional, tagged with
the optimistic revision and preview-dirty state — and reconciles production
panel surfaces exactly as it does from committed plans. The editor supplies
snapshots; it never publishes surfaces. Alternatives: (a) shell applies only
committed profiles (no live WYSIWYG preview until later), (b) editor renders
its own canvas only (what exists today). Proposed default: ship (a)+canvas
first; the subscription is a separate shell-lane slice.

**Q1.2 — Preview lifetime/cancel/crash.** Proposed invariants: a preview
exists only while an editor process holds the exclusive coordinator lease
and stays connected; lease loss, editor exit, or editor crash causes the
shell side to discard provisional snapshots and re-reconcile from the last
committed profile (repository already guarantees `CancelPreview` restores
the exact pre-preview profile in one revision); the shell never persists
provisional state and never adopts a preview on timeout. Alternative
considered and rejected: shell auto-commits a preview after lease timeout
(silently persists an unconfirmed layout).

**Q1.3 — Ordinary editor app vs LayerShellQt production surfaces.** Proposed
boundary: `qindaqt-settings`/customize is an ordinary Wayland top-level that
links only public `shell_customization`, `shell_layout`, and SDK values; the
only component mapping editor output to LayerShellQt surfaces is the
production shell via the Q1.1 subscription. Rejected alternative: a
layer-shell docked editor window (contradicts the manager's accepted
"no layer-shell settings UI" decision for the settings app,
`persistent-notification-quieting/1787796417-manager-boundary-decision.md`;
confirming the same rule extends to the customize editor).

## Owned and potentially colliding paths

- Shell lane owns: `src/shell_surface`, `src/shell_orchestration`,
  `src/shell` (runtime), and their tests/wiki. I own nothing there.
- My proposed paths: `src/apps/settings_center/editor/**` (S5; post-Settings1
  integration), `src/appshell`, `src/controls`, `src/design_tokens`.
- Collision risk today: none — no shared file edits proposed. Q1.1, if
  accepted, adds a shell-owned public API that my editor would consume.

## Safe to continue before the answer?

Yes. Slice S5 builds the editor UI against the repository/`evaluate()`
contract with the canvas preview (alternative (a)); the live-preview
subscription is sequenced as a follow-up shell slice and is not on my
critical path.

## Evidence or decision requested

An on-board reply accepting or amending Q1.1–Q1.3, naming the owning module
for the subscription API and the wiki section/ADR that will record it. If the
answering lane prefers, the reply may land in the future shell outcome
thread with a link back here.
