# Anika Rao — AppShell current-base merge rehearsal result

- Time: 2026-08-28T13:05:36Z
- Outcome: QQ-006.03 AppShell S0
- AppShell checkpoint: `de52a04966763cc11f8a551c58bd76ca38694c5c`
- Public checkpoint: `9db68c4023257b49421101fa1b13c73bbc2cfa85`
- Exact merge base: `1b4e2846e40d31d79ffb03db2229c07ff9bca271`
- Status: read-only rehearsal complete; no product edit, index/ref move, merge
  commit/object, configure, build, test, or runtime action occurred

## Exact disjoint manifests

Relative to the common base, public changes 36 paths and AppShell changes 26.
Exactly 33 public-only paths retain their public blob identities; the sorted
`status\\tblob\\tpath` manifest hashes to
`14aa91a47adad6fd09a63d3d5a517fcc5dda77df611276eb0b8d849415b6bdc5`.
These are the eight public status/roadmap/display-doc paths, 17
`src/services/display_service/**` paths, and eight
`tests/services/display_service/**` paths.

Exactly 23 AppShell-only paths retain their AppShell blob identities; the same
manifest format hashes to
`ec9d9a27328245f38bb42f1e56e86cf112dbd9e7125ec35fe138c796d93c8e53`.
These are AppShell ADR/app/wiki navigation, all eight `src/app_shell/**`
paths, and all ten `tests/app_shell/**` paths. The remaining three paths are
the only both-changed seam.

## Exact both-changed manifest and union

| Path | Base blob | Public blob | AppShell blob | Required union |
| --- | --- | --- | --- | --- |
| `docs/wiki/architecture/module-boundaries.md` | `56f63fbb` | `9818722b` | `6e3e3b20` | retain the AppShell row/dependency paragraph and the Display service row/exact-owner paragraph |
| `src/CMakeLists.txt` | `f9da42e6` | `8cd172c3` | `531a38e2` | retain `app_shell` after `controls` and `display_service` after `display_transaction` |
| `tests/CMakeLists.txt` | `3a98fba8` | `4034690d` | `3fc14b7e` | retain the same two focused test registries at their independent dependency positions |

The read-only legacy `git merge-tree BASE PUBLIC APPSHELL` rehearsal reports
exactly three `changed in both` sections and zero conflict-marker or conflict
diagnostic lines. Each hunk is positionally disjoint and the semantic union
above is mandatory even if a future Git version chooses a different textual
context.

## Preservation assertions

- Controls source/test tree IDs are exactly `a824c8b8` / `c97fb3ae` in base,
  AppShell, and public.
- QST source/test tree IDs are exactly `ef078d68` / `fd822c49` in all three.
- Text Editor source/test tree IDs are exactly `b07e053b` / `e471b9af` in all
  three.
- All six Notification source trees and all six corresponding test trees have
  identical tree IDs across the three commits. Public commit `0a547df` records
  qualification after AppShell's base; its status/task/feature changes are in
  the public-only manifest and therefore remain public-parent truth.
- Every Display D2 service source/test path is public-only and therefore keeps
  the exact `9db68c4` blob. AppShell has no Display service path.

The merge commit, when independently accepted and authorized, must be created
with first parent the then-current public commit (rehearsed here as
`9db68c4023257b49421101fa1b13c73bbc2cfa85`) and second parent the accepted
AppShell descendant (currently only checkpoint `de52a049`). Before execution,
the manager must refresh the public SHA and repeat the three-path seam check;
this audit authorizes no merge.

Juno's source findings arrived as this audit closed, so source repair now
preempts any further integration preparation.
