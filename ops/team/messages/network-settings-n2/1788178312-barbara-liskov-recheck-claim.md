# Barbara Liskov — Network Settings N2 repaired exact candidate: recheck claimed

- Timestamp: 2026-08-31T06:11:52-06:00
- Reviewer: Barbara Liskov (independent exact-commit reviewer, different worker
  from the implementer)
- Exact candidate under recheck: `6f5d0ba9915851195a4776b3a1e2f224c369a958`
- Tree: `38bfff6a125735166d4bec9d56fb134646d0ea12`
- Sole parent: rejected immutable candidate
  `43b563cdfe08455269375e2c356112902e562641`
- Original base / merge base with `main`:
  `9b3d65542c87b2b977482ed7e72e4425c5332dd6`
- Review worktree:
  `/home/cabewse/work_SPaC3/container-wm-workers/network-settings-n2-liskov-review`
  (detached, exact, tracked content byte-clean)

## Provenance verified before review

- `git rev-parse HEAD` = `6f5d0ba9915851195a4776b3a1e2f224c369a958`
- `git rev-parse HEAD^{tree}` = `38bfff6a125735166d4bec9d56fb134646d0ea12`
- `git rev-parse HEAD^` = `43b563cdfe08455269375e2c356112902e562641` (single parent)
- `git rev-list --count 43b563c..6f5d0ba` = 1
- `git status --porcelain` = empty
- `git diff --check 43b563c 6f5d0ba` = exit 0
- Repair diff is bounded: 16 paths, +502 / -66.

## Scope of this recheck

I will reproduce all four former P2 findings directly against this exact tree
rather than reading the repair prose, recheck every former P3 point, and search
for new safety, correctness, secrecy, lineage, replay, lifetime, accessibility,
package, and documentation defects introduced by the repair itself. I will
confirm the route still consumes only the public Network client, protocol, and
declared public model boundaries.

Gates: fresh strict Debug and Release configure/build, the complete 14-row
Network and Settings selector in each profile, the direct public-client package
policy 5-row selector in each, the mutation-focused 5-row proof,
`tools/validate-docs`, pinned strict MkDocs, `tools/check-source-shape`,
`git diff --check`, an exact dependency and changed-path audit, mergeability
against current manager main `ab203cac213b4bff882151de2398b4c1b46c99cb`, and
residue/clean-tree checks. Host display, Wayland, session bus, NetworkManager,
radio, and credential state stay out of the environment; builds use external
temporary roots.

## Lane ownership

I now exclusively own the serialized compiler, CTest, and private-bus lane for
this recheck and will explicitly release it in my terminal verdict.

## Note on the thread

`ops/team/messages/network-settings-n2/1788175945-radia-perlman-repair-handoff.md`
is not present in this detached review worktree; it exists on manager main
`ab203cac213b4bff882151de2398b4c1b46c99cb`. I read it from there. This is a
worktree-visibility artifact, not a finding.

Acceptance, if granted, will apply only to
`6f5d0ba9915851195a4776b3a1e2f224c369a958`. I will not approve prose or a
different SHA.
