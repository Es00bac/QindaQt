# Barbara Liskov — Network Settings exact reviewer

- Provider/model: Anthropic Claude, Opus 5 (1M context), `claude-opus-5[1m]`
- Role: Independent Network Settings N2 exact-commit reviewer
- Status: available — exact review of `43b563c` complete and terminally rejected
- Outcome: different-worker exact-commit review of the production Network
  Settings route over the public Network1 client boundary
- Branch: detached review checkout
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/network-settings-n2-liskov-review`
- Exact candidate: `43b563cdfe08455269375e2c356112902e562641`
- Tree: `31ec5f7acb312bd2f53884351fc6fdaa1b84dd5f`
- Base: `9b3d65542c87b2b977482ed7e72e4425c5332dd6`

## Observed strengths

- Exact-commit review grounded in executed evidence: fresh strict Debug and
  Release builds, the complete focused selector in both, direct hostile probes
  against the candidate's own boundary gate, and purpose-built reproductions
  rather than source-reading alone.

## Updates

- 2026-08-31T04:35:00-06:00 — Claimed independent review of exact Network
  Settings N2 candidate `43b563cdfe08455269375e2c356112902e562641`, tree
  `31ec5f7acb312bd2f53884351fc6fdaa1b84dd5f`, detached and clean on sole parent
  and merge base `9b3d65542c87b2b977482ed7e72e4425c5332dd6`. Read `AGENTS.md`,
  module boundaries, the Network1 architecture page, Settings Center route
  patterns, and Radia Perlman's exact handoff.
- 2026-08-31T04:44:00-06:00 — Source audit of all 38 changed paths found no
  private-service, libnm, Qt D-Bus, radio, or credential reach-through; the
  route's only non-Qt includes are public `network_client`, `network_protocol`,
  and `network_model` headers. Confirmed `NetworkClient::start()` is idempotent
  so page Reload works after `main.cpp` starts the client, and that the CLI
  rejects an unknown `--page` before any transport or model is constructed.
  Fresh strict Debug and Release configures and builds completed with exit 0
  and zero warnings.
- 2026-08-31T04:46:00-06:00 — Complete 14-row focused Network/Settings selector
  passed 14/14 with exit 0 in both strict Debug (25.53 s) and strict Release
  (25.05 s), including the boundary, boundary-poison, route-construction, and
  relocated installed-package rows.
- 2026-08-31T04:48:00-06:00 — Material finding: direct hostile probes against
  the candidate's own committed `check_boundary.cmake` showed the QML guard
  denies only `TextField`, so bare `TextInput`, `TextEdit`, and `TextArea` all
  pass, and the C++ five-name denylist admits a renamed
  `Q_INVOKABLE enableWifiRadio(bool)` or `submitKey(QString)`. That contradicts
  ADR-0055's accepted consequence. Separately built and ran an executable
  reproduction proving Scan/Connect/Disconnect project as available while an
  authoritative snapshot refresh is outstanding, yet every dispatch is refused
  with "The network service is not ready" (1/1 reproduced).
- 2026-08-31T04:50:22-06:00 — Terminally **REJECTED** exact
  `43b563cdfe08455269375e2c356112902e562641`, tree
  `31ec5f7acb312bd2f53884351fc6fdaa1b84dd5f`, with P0 0 / P1 0 / P2 4 / P3 6.
  Blocking: enabled-but-refused actions during snapshot refresh; the wiki
  asserting owner/epoch/revision visibility the QML never renders; the
  projection publishing an access-point BSSID while the same page states the
  route never exposes hardware addresses; and the boundary gate not enforcing
  ADR-0055's stated credential/radio consequence. No P0/P1 and no boundary,
  secrecy, lineage, or replay defect — the functional core is sound and every
  repair is small and local. `tools/validate-docs` (112 documents),
  `mkdocs build --strict`, `tools/check-source-shape` (1678 files, three
  pre-existing unrelated warnings), and `git diff --check` all exit 0.
  Noted for the manager that `git merge-tree` against `main` conflicts
  additively in `docs/wiki/adr/index.md` and `mkdocs.yml` against the newer
  ADR-0053 rows. Posted
  `ops/team/messages/network-settings-n2/1788173422-barbara-liskov-review.md`
  and requested the exact reproduction be routed back to Radia Perlman for
  repair in the same worktree, with a different-worker recheck of the repaired
  exact commit. Review worktree left detached, exact, and with tracked content
  byte-clean; no product source was edited, amended, or committed.
