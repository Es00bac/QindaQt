# Claim: Network N0 bounded pure protocol/model/client boundary

- Worker: Veda Park (GLM `zai-coding-plan/glm-5.3`, reasoning high)
- Timestamp: 2026-08-28T18:06:23Z
- Thread: platform-services / QQ-005.04 (Network connectivity) N0 slice
- Plan basis: `1787853847-samira-cole-plan-handoff.md` outcome C (Network1) and
  the PB-0/D1 pure-slice precedent

## User-visible outcome

A bounded, pure Network N0 foundation suitable for a later NetworkManager
adapter and shell/Settings UI: typed connectivity/radio/device/AP-identity
values that cannot carry secrets, known-network intent, scan leases,
owner/epoch/revision lineage, bounded deadlines, atomic snapshot publication
with stale/out-of-order rejection, and truthful unavailable/degraded/busy/error
states. An owner-bound client exercises this boundary entirely through an
injected fake transport/backend.

## Exact base and scope

- Base: `146fc48358c2659436dec4fc6b6062d23c5ee746` (manager integration HEAD)
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/network-n0-glm-veda`,
  branch `worker/network-n0-glm-veda`
- Owns only: `src/services/network_protocol/**`, `src/services/network_model/**`,
  `src/services/network_client/**`, matching `tests/services/network_*`,
  smallest additive CMake registration, an owning Network architecture page,
  ADR, protocol reference page, mkdocs/testing/module-boundary doc updates, and
  my board files.

## Boundary commitments

- N0 has no NetworkManager dependency, no host network access, no radio
  mutation, no credential store, no D-Bus, no GUI. Only injected fake
  transport/backend in tests.
- Never touches features.json/TASK_LIST/HANDOFF/provider state, Bluetooth,
  Audio, Display, Settings, Shell, apps, or integration branches.

## Completion evidence planned

- Strict-warning Debug and Release focused builds of the three targets.
- Hostile QtTest coverage for identity normalization, secret redaction, scan
  lease lifecycle, A/B/A owner change, revision rollback, timeout/late reply,
  atomic publication, intent validation, and teardown.
- An installed public-header consumer test and direct QtTest totals.
- `tools/check-source-shape`, `tools/validate-docs`, strict MkDocs, link
  checker, whitespace, provenance, and clean tree.

## Collision and dependency risks

- None observed: no other worker claims `src/services/network_*` paths; the
  platform-services plan deliberately leaves Network1 independent of Audio,
  Power, and Bluetooth bases. Shared-file edits are limited to additive CMake
  subdirectory lines and wiki navigation/boundary rows.
