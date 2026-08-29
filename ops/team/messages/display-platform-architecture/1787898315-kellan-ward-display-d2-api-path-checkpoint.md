# Kellan Ward — Display D2 API and path checkpoint

- Time: 2026-08-28T06:25:15Z
- Exact base/worktree: `7da3300cbe9a22fda077a07ff94b03b7adad396f` in `/home/cabewse/work_SPaC3/container-wm-workers/display-d2`
- Status: working; first product edit follows this checkpoint
- Runtime/build evidence: none yet; no compiler, test binary, private/nested/session, or host display action

## Accepted service contract

The service consumes complete typed D0 inventory frames `(uniqueOwner, outputGeneration, outputs)` and a borrowed/owned-through-composition D1 transaction port. A fresh injected Display1 epoch is created for the first accepted frame of each unique-owner lineage. The public revision is that lineage's positive D0 `outputGeneration`; it is never compared across owners. Equal generation requires exact typed inventory equality, a lower generation rejects, a strictly newer generation must change content, and every rejection preserves the current public snapshot and D1 machine. Upstream owner change establishes a new lineage/epoch; explicit transport loss clears the snapshot and destroys the machine so no stale value is served.

D0 current-output projection uses `display_identity::resolve` with absent EDID, so connector fallback is the only persistent-identity authority in this slice; D0 runtime UUID remains bounded metadata and is never promoted or hashed into `stableId`. Projection publishes only the observed current mode, preserves integral geometry/transform/scale/metadata, derives deterministic canonical priority/primary, and computes `liveFingerprint` through `display_topology::candidateFromSnapshot` plus `canonicalFingerprint`. A D0 frame that cannot form a valid D1 snapshot rejects atomically.

The service model alone routes snapshots into the accepted D1 state machine: output-set changes during an active transaction go to `topologyChanged`; same-set change while staged goes to `externalIntentObserved`; other active observations go to `observedSnapshot`. Stage/preview/confirm/cancel and apply completions remain D1-machine transitions. The resident composition owns one real monotonic clock, injected inventory/transaction ports, timer scheduling, D-Bus object/name, and teardown ordering. The packaged default transaction port cannot journal or apply, so preview fails closed until a separately accepted public output-management adapter is injected; there is no KWin private ABI.

## Exact path plan and coordination points

Owned new product paths are under `src/services/display_service/**` and `tests/services/display_service/**`. They comprise bounded inventory values/JSON decoder/projector, service model and ports, Compositor1 async inventory source, resident D-Bus object/process, XML/activation/systemd descriptors, and focused adapter/model/resident/descriptor tests. Primary documentation edits are `docs/wiki/architecture/display-service.md` and `docs/wiki/reference/display1-v1.md`.

Shared edits will be the smallest additive entries in `src/CMakeLists.txt` and `tests/CMakeLists.txt`, plus one new `src/services/display_service` row and dependency bullet in `docs/wiki/architecture/module-boundaries.md`. No MkDocs navigation entry is needed because both existing Display pages are already registered. I will post exact shared hunks at midpoint so integration preserves concurrent lanes.

## Evidence plan

Focused tests cover hostile D0 decode/projection; output add/remove/change; equal-generation changed-content rejection; owner change/new epoch; transport-loss reset; stale candidate fencing; preview/confirm/rollback ownership through fake ports; unavailable resident startup; and descriptor/XML parity. After source/static review I will request compile capacity for serial fresh Debug and Release focused tests, ASan+UBSan if practical, staged package/descriptor/installed-header consumer proof, strict docs/link/source/whitespace/forbidden-dependency gates, then create one exact immutable candidate and request a different-worker review.
