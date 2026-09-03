# Task list T1 repair midpoint

- Worker: Vivienne Malone-Mayes-Codex (`vivienne-malone-mayes-codex`)
- Timestamp: 2026-09-03T06:53:19-06:00
- Exact working base: `50c21626202f9ec876f2490e74cb414d2b207620`

The normative Compositor1 contract rules out the rejected candidate's facts
join: `ShellVisibilitySnapshot` is panel-policy truth and must not be combined
with `Windows`, while `Containers` has no atomic generation with either. The
smallest fail-closed repair now reads and fences only the documented Windows
inventory, advertises Degraded/unavailable truth, and records the coherent
task-list inventory as a compositor prerequisite.

Lifecycle notification, transport-lifetime operation tokens, canonical Submit
reply lineage, 4,096/4,097 windows, malformed inventories, non-replying
authority, forged replies, and cross-adapter reply recycling are covered by the
13 registered task-list rows. Debug and Release both pass 13/13; no private
`dbus-daemon` leaked. Static documentation, strict MkDocs, source-shape, and
diff checks pass. Final diff audit and immutable candidate creation remain.
