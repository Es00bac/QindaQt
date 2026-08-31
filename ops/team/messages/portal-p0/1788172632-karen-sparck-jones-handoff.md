# Portal P0 exact candidate handoff

- From: Karen Spärck Jones
- At: 2026-08-31T04:37:12-06:00
- State: handoff, not live
- Exact base: `9b3d65542c87b2b977482ed7e72e4425c5332dd6`
- Exact candidate: `b2271491239401adf1e4fffaed4fa57426e2a9ed`
- Branch/worktree: `worker/portal-p0` at
  `/home/cabewse/work_SPaC3/container-wm-workers/portal-p0`

## Outcome

The candidate adds the first production QindaQt appearance-only XDG portal
backend. It implements the installed standard
`org.freedesktop.impl.portal.Settings` version 1 endpoint with exact
`color-scheme` (`u`), `contrast` (`u`), and `accent-color` (`(ddd)`) values,
projected from a public exact-owner/epoch Settings1 Ready snapshot through the
public QST theme boundary. Loss, malformed input, unknown themes, stale
lineage, and non-representable QST accents withdraw complete readable truth.

The resident activation package contains the exact executable, public
policy/source libraries and headers, required QST themes, D-Bus descriptor,
hardened systemd user unit, Settings-only `.portal` metadata, and selector.
The standard signal's lack of an unset form is handled without fabricated
values: methods fail closed on loss and a replacement emits only values that
differ from the last signalled policy.

Changed paths are confined to new `src/services/portal/**`, focused
`tests/services/portal/**`, additive `src/CMakeLists.txt` and
`tests/CMakeLists.txt` seams, the primary portal architecture/reference pages,
ADR-0054, testing/overview/boundary/index updates, and additive `mkdocs.yml`
navigation.

## Exact evidence

- Strict Debug configure/build: exit 0 with KWin/shell disabled and the
  pre-existing private KDecoration3 prefix; portal policy/service/process test
  targets and both resident executables compile under warnings-as-errors.
- Debug `ctest -R '^qindaqt\\.portal-' --output-on-failure --no-tests=error`:
  exit 0, 7/7.
- Fresh strict Release configure/build with the same feature boundary: exit 0.
- Release identical portal selector: exit 0, 7/7.
- `./tools/validate-docs`: exit 0, 113 Markdown documents and navigation.
- `mkdocs build --strict --site-dir /tmp/qindaqt-portal-p0-docs`: exit 0.
- `./tools/check-source-shape`: exit 0, 1,684 files; its three warnings are
  pre-existing files outside this candidate's owned paths.
- `git diff --check`: exit 0. Assigned-base merge base is exact. Product
  worktree is clean and no exact portal/Settings fixture process remains.

The seven rows cover pure/hostile projection, exact Settings1 owner and stale
reply fencing, real private-D-Bus signatures/filter/errors/signals/name
rollback, two-daemon activation and exact-process exit/restart, source poison,
negative self-proof, and staged installed package/private lifecycle/poison.

## Bounded caveats and next action

This does not contact or qualify the host session bus, installed package set,
portal frontend, or toolkit behavior. It claims no chooser, OpenURI,
notifications, inhibit, screencast, remote desktop, consent UI, or other portal
family. The direct docs-link CTest selector is not registered in this build;
the repository's authoritative validator and strict MkDocs commands above both
pass.

Please assign a different worker to review the immutable exact candidate
`b2271491239401adf1e4fffaed4fa57426e2a9ed`, including protocol signatures,
source-truth/lineage failure behavior, activation/install metadata, hostile
poison effectiveness, and replay of the seven focused rows.
