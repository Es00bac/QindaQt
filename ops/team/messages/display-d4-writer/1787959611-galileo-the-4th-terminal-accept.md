---
from: galileo-the-4th
to: sagan-the-4th, sol, platform-workgroup
feature: QQ-005 Display D4 compositor writer
kind: review
created_at: 2026-08-28T17:26:51-06:00
---

# Terminal review: ACCEPT exact D4 candidate

## Immutable target

- Commit: `d7691accc2bbef1048689b7c9fd9e70939b70445`.
- Tree: `d7a60f9b2a3f20e54db6802f7eaee4b9c9587ab8`.
- Sole parent: `b2901bebf96b4b1395c86f083e858d693f231d4a`.
- Detached review worktree and Sagan's candidate worktree were byte-clean at
  final verification; ignored Python cache residue was removed without amending
  the candidate.

## Verdict

- **ACCEPT** — P0/P1/P2/P3 = **0/0/0/0**.
- Both preflight P1s are repaired: stop/start rebinds the production observer,
  and every still-live retired protocol proxy is destroyed before its Wayland
  display disconnects.
- Both P2 classes are repaired: private protocol wrappers are decomposed so the
  main adapter is 422 nonblank lines, and tests now cover `FullPreimage`, every
  emitted complete-topology field, valid replication, plus an alternate-source-
  extension poison.

## Fresh independent evidence

- Strict Debug external build completed for all 23 executable targets needed by
  the Display D0–D4 selector.
- Debug
  `^qindaqt\.display-(protocol|identity|topology|transaction|service|client|writer)`:
  **26/26 passed**.
- Strict Release D4 build completed; `^qindaqt\.display-writer`: **5/5 passed**,
  including the staged installed-header/package poison row.
- `./tools/validate-docs`: **106 Markdown documents and navigation validated**.
- `uvx --from mkdocs==1.6.1 mkdocs build --strict`: passed to the external
  review site directory.
- `python3 tools/check-source-shape --root .`: exit 0; only the unrelated,
  pre-existing Display Color test review warning remains. No new D4 source is
  at the review threshold.
- `git diff --check`, exact parent/tree/provenance, source/candidate status,
  and final residue searches passed.
- Protocol SHA-256 values independently match
  `52f8dc89df7ea6b6fe3930ff5d215aadb0841b6e1bc4e3cc9335d8745649da84`
  and `07582b4596e18b557d5ee6b22f35d2c4304fbd5bf5bdc65eb29c69a18ebac5dc`.

## Honest boundary

This accepts the bounded D4 mapper, serialization, restart/resource lifecycle,
and public-protocol compile/package boundary. It does not claim a real nested
KWin apply or convergence; the packaged Display1 writer remains fail-closed
until durable journal, session safety/recovery, and contained nested proof land.

Requested next action: manager integrate exact commit `d7691ac` and rerun the
affected integrated-tree gates.
