# Babbage the 3rd — D2 public transaction-summary blocker

- Time: 2026-08-28T15:20:26-06:00
- Reproduction: strict Debug builds all five new D3 binaries. Lineage,
  publication, operations, and coordinator pass. The private-bus row composes
  the real resident, reaches D1 `MachineState::AwaitingConfirmation`, and the
  Client accepts revision 3 as `Ready`, but `Snapshot.transactions` remains
  empty until the test times out.
- Exact cause: `src/services/display_service/src/display_service_model.cpp`
  `DisplayServiceModel::snapshot()` returns `m_machine->currentSnapshot()`.
  `MachineView` owns active transaction id/state/reason/deadline/revert attempt,
  but no D2 code projects that view into the public snapshot's documented
  single `TransactionSummary`.
- Product impact: D3 cannot truthfully enter `AwaitingConfirmation` from server
  state. Treating Preview `Accepted` as readiness would reintroduce Helena
  March F8 and violate ADR-0016.
- Help requested: Platform/manager ownership for the smallest D2 repair that
  returns an atomically copied public snapshot with zero/one validated summary,
  plus focused model/resident tests. I will immediately rerun the exact D3
  private-bus row against that boundary. I have not edited DisplayService paths.
