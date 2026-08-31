---
author: Radia Perlman
timestamp: 2026-08-31T04:14:11-06:00
topic: network-settings-n2
type: midpoint
---

# Midpoint: Network Settings public-client boundary is executable

The production route now exists in the isolated worktree and remains below the assigned authority ceiling:

- `src/apps/settings/network/**` contains one model over public `NetworkClient`/protocol values and five QST/Controls presentation components. It has no D-Bus, service, libnm, NetworkManager, profile editor, radio mutation, password field, or secret input.
- The Settings Center has one additive closed route (`network`, Ctrl+4), one process-lifetime public Qt Network transport/client/model composition, and exactly-one-loader wide/compact hosting.
- Inventory is derived on demand from the client's accepted exact owner/epoch/revision projection. Owner loss clears it; degraded retained truth is explicitly stale/read-only; operation results do not manufacture connectivity; uncertain work is never replayed.
- Only scan, stored-known-network connect, and active-device disconnect cross the UI facade, after the public model admits them. The page explains that credentials remain with an external NetworkManager secret agent and provides no credential-entry path.

Fresh strict Debug evidence at `/tmp/qindaqt-radia-network-debug`:

- production model/QML/executable compile: pass;
- `qindaqt.network-settings-model`: pass;
- `qindaqt.network-settings-model-adversarial`: pass;
- `qindaqt.network-page`: pass;
- positive and five-control negative boundary/poison rows: 2/2 pass;
- Settings route registry/controller/offscreen navigation regressions: 3/3 pass;
- combined focused selector: 8/8 pass.

The next gate is same-change ADR/wiki truth plus installed `--page network` and developer-tree-withheld package proof, followed by fresh strict Release and repository quality gates.
