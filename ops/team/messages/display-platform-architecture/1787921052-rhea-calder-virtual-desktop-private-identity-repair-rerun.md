# Rhea Calder — private identity repair and exact rerun

- Timestamp: 2026-08-28T12:44:12Z
- Exact HEAD: `3320afdb4afad1c396b85add576f60d59e1d3b57`
- Tree: `b5664f1e65a3d3984d88157c8083533956fa0462`
- Parent: loader repair `e2ab439c79277464ebd9a9a8cba7d44b502cf17e`

The second non-amended repair creates per-run passwd/group files containing
only the mapped qualification UID/GID and `/home/qindaqt`, authenticates their
exact path, caller ownership, regular-file shape, and contents with the run
identity, then read-only binds them into the empty root. Host account databases
remain absent. Focused identity tests are decomposed into their own file.

Safe exact-tree evidence:

- private-identity units: **2/2 PASS**;
- complete desktop-session Python units: **45/45 PASS**;
- ten Python sources compiled in memory: PASS;
- `desktop.virtual.sandbox-unit` and package contract: **2/2 PASS**;
- documentation/navigation: **64 PASS**;
- source shape: **994 files**, zero warnings/issues;
- whitespace and clean tree: PASS.

No compiler or private process now competes; available RAM is 14 GiB. Rhea is
starting only the same acknowledged `desktop.virtual.boot.1080p` command on
this immutable HEAD. Required success remains authenticated topology, exact
PSS ceiling/schema, fresh logs/result, zero screenshots by contract, and exact
owned teardown with no run-root residue.
