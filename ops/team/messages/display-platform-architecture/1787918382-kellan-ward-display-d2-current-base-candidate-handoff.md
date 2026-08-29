# Kellan Ward — Display D2 current-public-base candidate handoff

- Timestamp: 2026-08-28T11:59:42Z
- Status: handoff/not live
- Candidate: `a5528f889d60b88b10a91b9b60d8d9e8d6e5e00e`
- Tree: `2f2036aa6bde15731f30885d70e8b21d9a44f4d6`
- First parent / exact public base:
  `0a547df33d9a31b969d78b4ca649d0b39dc04797`
- Second parent / exact qualified D2 repair:
  `241c00b3567463001a3eaa3f5c60ba9134cce429`
- Preserved failed predecessor:
  `8901f23fe159263522e2e0d76278c4786c8375e5`
- Delta to first parent: 32 paths, +3,485/−29
- Sorted first-parent path-manifest SHA-256:
  `8eed5cc89018eced9adf7ea2a1751ae9ccbed85438a0d44b617355cbb680394c`
- Worktree: clean

## Merge result

The exact common ancestor of public and repair is original D2 base
`7da3300cbe9a22fda077a07ff94b03b7adad396f`. Public changes 83 paths from that
base; the D2 history changes 32. Their only shared path is
`docs/wiki/development/testing-harness.md`.

Git merged that page automatically without conflict. The first-parent diff adds
only D2's 24-line private Display1 lifecycle section at its current public
location. The second-parent diff retains all current Notification Live/focus,
supervisor replacement, Settings outage, private-lock, fractional-scale and
race evidence already on public. Exact path-level audit proves:

- all 82 public-only paths in the result are byte-identical to public
  `0a547df3`;
- all 31 repair-only paths are byte-identical to repair `241c00b`;
- the one shared wiki path contains both non-overlapping additions;
- zero unmerged or unstaged paths remain; and
- both exact parents are ancestors, in required first/second order.

No CMake, source, descriptor, XML, test registry, or compiled product path was
changed by both parents. No manual semantic resolution or product edit was
required. The exact Debug/Release/sanitizer/private-bus/package qualification
therefore remains attached to second parent `241c00b`; this merge did not rerun
compiler or private runtime.

## Current-tree static evidence

All commands exited 0:

- `git diff --cached --check` before commit;
- `python tools/docs_validation.py` — 63 documents/navigation;
- `./tools/check-source-shape` — 1,002 source files, zero findings;
- `uv run --with-requirements docs/requirements.txt python -m mkdocs build
  --strict --site-dir build/d2-current-base-docs-1787918222`;
- Display1 XML parse and activation/systemd descriptor name/path parity;
- exact additive `src/CMakeLists.txt` and `tests/CMakeLists.txt` service
  registrations plus both private-runtime CTest registrations;
- forbidden D2 dependency, new-file SPDX, first-parent manifest, ancestry,
  unmerged/unstaged and clean-state gates.

## Requested review

A different worker should review exact merge commit
`a5528f889d60b88b10a91b9b60d8d9e8d6e5e00e`, not this summary. Verify exact
tree/parent order, public-only/repair-only byte preservation, the sole combined
testing-harness page, current Notification Live plus D0/D1/Power/Controls/Text
contract retention, D2 descriptor/build/test registrations, and the decision
not to rerun compiled/runtime gates when no compiled path overlapped. Manager
alone integrates after both the exact repair and exact merge reviews pass.
