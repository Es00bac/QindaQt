# Gauss Meridian — claim: D2 public transaction-summary projection repair

- **Timestamp:** 2026-08-28T15:27:16-06:00
- **Supervisor/outcome owner:** Babbage the 3rd (Display D3
  client/coordinator)
- **Provider/model:** Z.AI coding plan `glm-5.3`, reasoning high
- **Base:** `146fc48358c2659436dec4fc6b6062d23c5ee746` (dirty shared
  worktree `/home/cabewse/work_SPaC3/container-wm-workers/display-d3-kimi-nyra`,
  branch `worker/display-d3-kimi-nyra`); every existing dirty byte from
  Babbage/Tara/Pavel/Nyra is preserved untouched.

## Outcome

`DisplayServiceModel::snapshot()` atomically projects the D1 machine view's
active transaction id/state/reason/deadline/revert attempt into exactly
zero or one validated public `Display::TransactionSummary` inside the
returned public snapshot, so D3 consumers can truthfully observe
`AwaitingConfirmation` (and every other active/terminal state) from server
state instead of inferring readiness from Preview replies.

## Planned ownership (exact paths)

- `src/services/display_service/src/display_service_model.cpp` and its
  public header if the accessor contract needs it
- `src/services/display_service/src/display_service_projection_p.h` (new,
  private pure seam, if introduced)
- `src/services/display_service/CMakeLists.txt` (only if a new private
  header must be listed)
- `tests/services/display_service/tst_display_service_model.cpp`,
  `tests/services/display_service/tst_resident_display_service_private_bus.cpp`,
  `tests/services/display_service/CMakeLists.txt` (focused model/resident
  rows for none/preview/confirm/revert/terminal/invalid states)
- `docs/wiki/architecture/display-service.md` and the
  `qindaqt.display-service-model` row in
  `docs/wiki/reference/display1-v1.md` if the contract needs clarification

## Explicitly prohibited (not touched)

`src/services/display_client/**`, `tests/services/display_client/**`,
`src/CMakeLists.txt`, `tests/CMakeLists.txt`, manager ledgers, other
worker profiles, host display/input/session/config, unrelated cleanup.
Never `git add -A`; if I commit, only explicitly owned files are staged.

## Completion evidence plan

Reproduction of the empty-`transactions` failure against the current dirty
tree; strict Debug focused build under
`/mnt/d/QindaQt/builds/display-d3-projection-gauss`; focused
`qindaqt.display-service-*` rows plus the Display D0–D2 regression set;
`tools/check-source-shape`, `tools/validate-docs`, `mkdocs build
--strict`, `git diff --check`; handoff names the exact commit and paths so
Babbage can immediately rerun the D3 private-bus row.

## Collision/dependency risks

The shared worktree carries Babbage's in-flight D3 work; my edits are
confined to DisplayService paths he does not own. `snapshot()` semantics
change for every consumer — the D3 client and resident object read the
same accessor, which is the point of the repair; I will keep epoch,
revision, fingerprint, and outputs byte-identical so only the previously
always-empty `transactions` list gains truth.
