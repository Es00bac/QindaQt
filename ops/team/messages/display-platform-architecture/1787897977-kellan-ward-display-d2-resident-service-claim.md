# Kellan Ward — Display D2 resident service/adapters claim

- Time: 2026-08-28T06:19:37Z (corrected Kellan-owned timestamp; see append-only correction `1787898089`)
- Persona: Kellan Ward, permanent Display implementer/lead
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/display-d2`
- Branch: `worker/display-d2`
- Exact base: `7da3300cbe9a22fda077a07ff94b03b7adad396f`
- Base tree: `3dd9812b8f8bfa6f9ba52f05ad3f66058f2ec740`
- Status: working; product source is not yet edited

## Accepted outcome and boundary

I claim the resident Display1 service/adapters slice over the integrated D0 inventory and accepted D1 protocol/identity/topology/transaction libraries. The service model will consume typed, injected inventory observations and an injected D1 side-effect/transaction port; own a fresh public epoch and monotonic public revision; discard stale state on upstream owner loss/change; reject changed content at an equal source generation; and preserve the D1 preview/confirm/cancel/revert state-machine authority. Identity projection remains deterministic and privacy-preserving, using only D1 public identity inputs and never promoting D0 runtime UUIDs to persistent identity.

The service, focused adapters/tests, activation/unit descriptors, and primary Display docs are mine. Shared source/test CMake and documentation navigation changes will be minimal additive hunks and called out at checkpoint. There will be no direct KWin private ABI, QML, Settings implementation, host compositor mutation, or nested/private-session evidence in this lane while Soren owns that runtime.

## Material scope fence

The accepted D0 `Compositor1.Outputs` schema supplies observable enabled-output state and `outputGeneration`, but it does not supply a public mutation transport or a complete mode inventory/mode IDs. This D2 slice must therefore keep production mutation unavailable/fail-closed until a separately accepted public output-management adapter is injected; it must not fabricate KWin authority or claim the full nested hotplug/restart program from the earlier end-state plan. In-process adapter/model tests will prove add/remove/change, transport-loss reset, equal-generation changed-content rejection, deterministic projection, and transaction ownership through injected ports.

Next checkpoint: exact public header contract, path plan, and the D0-to-D1 projection/lineage decision before build registration changes.
