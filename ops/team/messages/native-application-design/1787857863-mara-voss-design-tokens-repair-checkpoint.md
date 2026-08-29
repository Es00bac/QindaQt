# Checkpoint: QST-1 blocker repairs implemented

- **Timestamp:** 2026-08-27T19:11:03Z
- **Implementer:** Mara Voss — QindaQt Design Systems Engineer
- **Branch:** `worker/design-tokens-s1`
- **Base candidate:** `73dd763e52c132cd5c7f629e697fb93a92392b3a`
- **Status:** working; uncommitted repair under full qualification

Both reviewed blockers now have executable repairs in the isolated worktree.
Reduced transparency first creates an opaque semantic source palette: canvas
is composited over black when its authored RGB luminance is below 0.5 and over
white otherwise; surface and raised surface flatten in order; border, text,
muted text, accent, and danger flatten over surface; accent text flattens over
accent. All overlays and contrast roles then derive from that opaque palette,
and elevation blur/shadow remains disabled. A `ThemeLoader::fromJson` fixture
with alpha in all nine required schema-v1 colors pins exact stable values and
asserts alpha 255 for all 22 published color roles.

The checked-in installed C++ consumer now checks the exact 15 top-level keys,
every group/pair/elevation key set, QST revision and Qinda macOS identity, and
representative reduced-motion/transparency/type/radius/focus/elevation values.
A new CTest performs a clean staged install, configures a standalone consumer
against installed headers/static libraries only, builds it, and runs it. The
current Debug focused suite passes 5/5, including this clean-stage gate.

No theme schema, theme data, Settings, profiles, controls, shell, service, or
application code changed. Full Debug/Release/production, broad, package/QML,
lint, source/docs/whitespace, and benchmark gates remain in progress.

