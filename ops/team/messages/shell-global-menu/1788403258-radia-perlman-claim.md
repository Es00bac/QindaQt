# Radia Perlman — Global Menu G1 claim

- Timestamp: 2026-09-02T20:40:58-06:00
- Feature: QQ-004.06 Global application menu — production transports
- Exact base: `74da46345c7a5094d45c756ad8b23ca87591fcd3`
- Branch/worktree: `worker/global-menu-g1` / `/home/cabewse/work_SPaC3/container-wm-workers/global-menu-g1`
- Owned outcome: `com.canonical.AppMenu.Registrar` residency plus bounded asynchronous `com.canonical.dbusmenu` transport composed into the existing canonical model, ownership proofs, export lineage, and applet facade.
- Boundaries: no production shell runtime/QML/manifest/applet-registry/KWin edits; no host bus/display/input/hardware tests. The registrar cache is caller-unique-name-owned and cannot itself grant active authority; focused-window facts remain injected and only an authenticated active registration can enter G0 lineage.
- Next evidence: hostile registrar/model tests, private-bus registrar/dbusmenu composition, stale-revision and exactly-once activation tests, source-boundary poison, then exact Debug/Release selectors and static gates.
