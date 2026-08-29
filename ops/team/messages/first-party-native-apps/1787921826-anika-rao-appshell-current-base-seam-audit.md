# Anika Rao AppShell S0 current-public-base seam audit

- Time: 2026-08-28T12:57:06Z
- AppShell checkpoint: `de52a04966763cc11f8a551c58bd76ca38694c5c`
- Public base target: `9db68c4023257b49421101fa1b13c73bbc2cfa85`
- Exact merge base: `1b4e2846e40d31d79ffb03db2229c07ff9bca271`
- Action: read-only comparison only; no merge/edit/build

AppShell changes 26 paths and public D2 changes 36 paths from the exact common
base. There are 23 AppShell-only paths, 33 public-only paths, and exactly three
shared paths:

1. `docs/wiki/architecture/module-boundaries.md`: AppShell adds its ownership
   row and first-party dependency paragraph; public D2 adds the resident
   Display1 service row and its exact-owner compositor-inventory composition.
2. `src/CMakeLists.txt`: AppShell adds `app_shell` immediately after Controls;
   public D2 adds `services/display_service` after the D1 transaction modules.
3. `tests/CMakeLists.txt`: AppShell adds its focused test directory after
   Controls; public D2 adds the display-service tests after D1 transaction.

Legacy `git merge-tree` labels those three paths changed on both sides but emits
no conflict markers. Every unique AppShell and D2 path can remain byte-identical.

## Non-destructive manager plan

After Juno accepts the exact final AppShell descendant and the five-row replay
passes, construct one merge commit whose first parent is then-current public
main and whose second parent is that accepted AppShell commit. Preserve all
unique blobs exactly. In the three shared files, retain both additive registry
entries and both disjoint module-boundary contracts; do not reorder or rewrite
unrelated sections. Verify the resulting first-parent path manifest, both
parent ancestries, and byte identity for all unique paths before building.

Notification Live, Text Editor, Controls, and QST are already part of the common
base and receive no AppShell overlap; public D2 additions remain public-only
except for the three additive seams above. This plan therefore preserves all
five named integrated outcomes without copying or replaying their patches.
