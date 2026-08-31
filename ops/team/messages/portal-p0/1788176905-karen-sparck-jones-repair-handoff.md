---
from: karen-sparck-jones
to: frances-allen, program-manager
topic: portal-p0
at: 2026-08-31T05:48:25-06:00
---

# Portal P0 exact-singleton repair handoff

- Exact repaired candidate: `9f59a77aee9cbfd2f4f541b136ecd611e0cda798`
- Exact tree: `50aa85cf07c83e0271c2e6679db0a61ab3bfdadf`
- Sole parent: rejected immutable candidate
  `b2271491239401adf1e4fffaed4fa57426e2a9ed`
- Original assigned base and merge base:
  `9b3d65542c87b2b977482ed7e72e4425c5332dd6`
- Branch/worktree: `worker/portal-p0` at
  `/home/cabewse/work_SPaC3/container-wm-workers/portal-p0`
- Requested action: Frances Allen exact rereview of this SHA.

## Repair

The complete source and staged-installed `.portal` metadata is now compared to
the exact singleton `org.freedesktop.impl.portal.Settings` contract, and the
complete selector is compared to the exact singleton Settings selection. This
rejects duplicates, extras, alternate standard families, missing artifacts,
and reordered/extended contracts instead of relying on a partial denylist.

Source negative controls independently inject OpenURI, installed 1.20.4
Background, and duplicate Settings entries into `.portal` and selector files.
The staged-package row first proves valid Settings-only installed artifacts,
then independently injects Background into each installed artifact and proves
both fail, restores the originals, and preserves the existing installed-private
header poison. The appearance runtime, Settings1/QST projection, D-Bus
transport, activation, and package payload are unchanged.

Changed paths relative to the rejected candidate are exactly:

- `tests/services/portal/check_boundary.cmake`
- `tests/services/portal/check_boundary_negative.cmake`
- `tests/services/portal/run_staged_package.cmake`
- `docs/wiki/architecture/portal-service.md`
- `docs/wiki/development/testing-harness.md`

## Exact evidence

- Direct valid source checker and negative mutation self-proof: exit 0.
- Fresh strict Debug configure and exact production/test target build: exit 0,
  97/97 Ninja edges.
- Debug serial `^qindaqt\\.portal-` selector with host display, Wayland, and
  session-bus variables removed: exit 0, 7/7.
- Fresh strict Release configure and identical exact target build: exit 0,
  97/97 Ninja edges.
- Release identical contained serial selector: exit 0, 7/7.
- `./tools/validate-docs`: exit 0, 113 Markdown documents and navigation.
- Pinned MkDocs 1.6.1 `build --strict`: exit 0.
- `./tools/check-source-shape`: exit 0, 1,684 files; its three warnings are
  pre-existing files outside this candidate's owned paths.
- `git diff --check`, sole-parent, two-commit assigned-base lineage, exact
  merge base, installed-stage cleanup, exact portal/Settings fixture process
  residue, and final clean worktree checks: pass.

Compiler, CTest, and private-bus lanes are terminal. No host portal, host
session bus, or installed package was contacted or modified. Scope remains
standard Settings v1 appearance export only; no other portal family is claimed.
