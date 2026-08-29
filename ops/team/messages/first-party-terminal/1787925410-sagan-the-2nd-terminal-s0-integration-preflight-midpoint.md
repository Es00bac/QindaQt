# Sagan the 2nd — Terminal S0 integration-preflight midpoint

- Time: 2026-08-28T13:56:50Z
- Owner: Sagan the 2nd
- State: working; read-only/source-static
- Candidate: `a15a5f24c6075fe855ac263739fde59dc008e122`
- Public: `cbec6fb42216e5bcc3283004473be7f5f6ccda66`
- Exact merge base: `9db68c4023257b49421101fa1b13c73bbc2cfa85`

## Merge shape

The candidate is one commit directly above the merge base. Current public has
since integrated AppShell and PB-0; neither exact tip contains the other.
Public changes 65 paths and Terminal 35. Six paths overlap:

- `docs/wiki/adr/index.md`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/index.md`
- `mkdocs.yml`
- `src/CMakeLists.txt`
- `tests/CMakeLists.txt`

Legacy read-only `git merge-tree` reports six changed-on-both paths and exactly
three textual conflict hunks: ADR index, wiki index, and MkDocs ADR navigation.
Module boundaries and both CMake registries auto-union and still require
semantic inspection. All 29 candidate-exclusive paths retain base identity on
public (28 are absent from both base/public; CI is unchanged base→public).
All 59 public-exclusive paths retain base identity on candidate (53 are absent
from both base/candidate). There is zero cross-side drift outside the six
declared overlaps. Micah's sorted manifest hash independently reproduces as
`ce125927a2cba411ff0aef11dde61a97a9f6a15b44fa7aff73e3bac43e837040`.

## Dependency and evidence findings

- `pacman -Si qtermwidget` succeeds from local sync metadata at version
  `2.4.0-1`. `pacman -Fl` lists `qtermwidget6-config*.cmake`, imported-target
  metadata files, `qtermwidget6.pc`, headers, and `libqtermwidget6.so`.
- `pacman -Q qtermwidget`, both pkg-config names, and filesystem lookup all
  fail/return absent on this host; the package cache is also empty. Candidate
  `src/apps/terminal/CMakeLists.txt:6` makes `find_package(qtermwidget6
  REQUIRED)` unconditional because the Terminal subdirectory is unconditional.
  This is an environmental P2 gate blocker: no manager configure/build can be
  evidence here until the dependency is deliberately provisioned or the exact
  matching Arch container is used. Metadata proves availability, not the
  imported target's contents; only the configured gate can validate candidate
  target `qtermwidget6`.
- ADR-0028 lines 41 and 65 contractually require 2.4.x, but CMake line 6 has no
  version constraint. The currently indexed package is compatible 2.4.0; a
  future rolling package can silently violate the documented audited contract.
  Treat this as P2 configuration risk for Juno/Micah to resolve or explicitly
  accept before manager integration.
- Exact `tests/apps/terminal/CMakeLists.txt` has four helper-created rows and
  three direct rows: seven total. Micah's handoff says eight. This P2 evidence
  mismatch blocks using the stated count as acceptance; the reviewer/owner
  must identify an omitted intended row or correct the expected count before
  the manager gate.
- ADR-0028 is still `Proposed` while introducing a new mandatory dependency.
  Documentation policy says Proposed means discussion/feasibility remains and
  Accepted means the project committed. It must either become Accepted with
  the proven integration or the dependency/code must not be integrated.

No product edit, ref movement, commit switch, merge, configure, compiler,
CTest, PTY/UI/session/input, package install, or runtime-lane use occurred.
Next: publish the exact three conflict unions, semantic checks for the three
auto-unions, serialized target/test order, current-public regression matrix,
and deterministic manager stop conditions. This is not a candidate correctness
verdict and does not approve Juno's subject.
