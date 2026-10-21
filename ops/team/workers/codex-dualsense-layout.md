# Codex DualSense layout

- Role: Controller layout implementer and integration owner
- Status: available — layout installed, profiles verified and source integrated on qinda
- Base: `24b89b00e75dc19c482954729edaa31c88126a45`
- Branch: `fix/dualsense-layout-20261020`
- Worktree: `.cache/dualsense-layout-20261020`
- Ownership: `src/controllers`, `tests/controllers`, controller Settings labels, controller wiki/ADR and bounded delivery
- Execution: current selected Codex session only; no delegated agents or external model workers

## Updates

- 2026-10-20T23:19:28+00:00 — Claimed exact user mapping: DualSense defaults, voice command capture, persistent gyro toggle and held Meta+Shift mouse actions. Preserve existing device motion preferences and other controller families. Acceptance: focused standalone build and controller tests, strict MkDocs/link checks, signed Portage delivery and Controllers1 readback.

- 2026-10-20T23:24:27+00:00 — Full map and separate DualSense defaults implemented; focused plugin/test build passes. New private held-input state orders Meta+Shift around mouse events. Existing installed desktop is r35; delivery must retain its complete signed image and rebuild only Settings Input and the controller plugin. ADR reserved as 0426 to avoid the newer animation branches.

- 2026-10-20T23:30:57+00:00 — Exact controller blobs retain six passing CTest groups/41 QtTest cases after replay onto installed r35 source `89e341e46`; strict MkDocs and 605-document validation pass. Fixed Sony capability projection and family-specific binding guidance in QML; offscreen real InputControllerSection capability smoke passes 3/3 with no hardware. Initial r37 packaging stopped before compilation; r38 pins final source `4e886968b`. Source and overlay checkpoints are on qinda.

- 2026-10-20T23:41:26+00:00 — Live Voice1 is enabled with command capability but retains a previous provider error. Installed Gabbee admits fresh starts from Idle/Error; the controller incorrectly admitted only Idle. Repairing that caller check with both-mode private-bus tests. The r38 Settings/image build continues unchanged; final delivery will add a standalone controller-only follow-up so no Settings rebuild is repeated.

- 2026-10-21T00:02:09+00:00 — Complete: integrated source d4c85a4b5, overlay 2dd46e9, signed r39 installed through Portage. Native controller plugin loaded; five profiles × 19 bindings read back with motion preferences/other families preserved. Six CTest groups/43 QtTest results pass, installed Input construction ready, 2,411 files/symlinks match the signed image, original compositor/shell alive. Shared catalog remains its previous baseline; explicit laptop delivery is r39. No delegated models, hardware button injection or live voice capture.
