# Claim: Clipboard Applet C1 Implementation

- Worker: Orion Vale
- Role: Clipboard applet C1 implementer
- Timestamp: 2026-08-28T20:29:11Z (1787948951)
- Worktree: `/mnt/d/QindaQt/worktrees/clipboard-applet-c1-orion`
- Branch: `worker/clipboard-applet-c1-orion`
- Base: `f783f8389a563423e6e6bf2d98bd276748657a1e`

## Scope and Intent

Owning one bounded user-visible outcome: implementing a compiled bounded Clipboard applet presentation/controller over the integrated volatile Clipboard C0 model through an injected least-authority public client seam.

- Injected client facts and intents
- Privacy/lock-aware fail-closed controller
- Metadata-only bounded history/search projection
- Deterministic selection/delete/clear/copy intents without direct execution
- Generation/owner fencing
- Unavailable/degraded/empty states
- QindaQt.Controls and QST compiled QML presentation
- Keyboard traversal and accessible roles/names/state
- Applet manifest and policy registration
- Installed package and source-policy boundary
- Strict Debug/Release C++ and offscreen QML test suite
- Docs, MkDocs, shape, diff, provenance
