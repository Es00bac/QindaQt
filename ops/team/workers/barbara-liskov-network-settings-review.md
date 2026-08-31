# Barbara Liskov — Network Settings exact reviewer

- Provider/model: Anthropic Claude, Opus 5 (1M context), `claude-opus-5[1m]`
- Role: Independent Network Settings N2 exact-commit reviewer
- Status: available — exact recheck of `6f5d0ba` complete and terminally accepted; compiler, CTest, and private-bus lane released
- Outcome: different-worker exact-commit review of the production Network
  Settings route over the public Network1 client boundary
- Branch: detached review checkout
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/network-settings-n2-liskov-review`
- Exact candidate: `6f5d0ba9915851195a4776b3a1e2f224c369a958`
- Tree: `38bfff6a125735166d4bec9d56fb134646d0ea12`
- Sole parent (rejected immutable candidate): `43b563cdfe08455269375e2c356112902e562641`
- Original base / merge base with `main`: `9b3d65542c87b2b977482ed7e72e4425c5332dd6`
- Prior exact review: `43b563c` terminally REJECTED (P0 0 / P1 0 / P2 4 / P3 6)
- Current exact review: `6f5d0ba` terminally ACCEPTED (P0 0 / P1 0 / P2 0 / P3 3)

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
- 2026-08-31T06:11:52-06:00 — Claimed the independent different-worker exact
  recheck of repaired candidate `6f5d0ba9915851195a4776b3a1e2f224c369a958`,
  tree `38bfff6a125735166d4bec9d56fb134646d0ea12`, sole parent the rejected
  immutable candidate `43b563cdfe08455269375e2c356112902e562641`. Verified
  exact SHA, tree, single-parent chain, empty `git status --porcelain`, and
  `git diff --check 43b563c 6f5d0ba` exit 0; the repair diff is bounded to 16
  paths, +502/-66. Read my original verdict, `AGENTS.md`, and Radia Perlman's
  repair handoff `1788175945-radia-perlman-repair-handoff.md` (present on
  manager main `ab203cac213b4bff882151de2398b4c1b46c99cb`, not in this
  detached worktree — a visibility artifact, not a finding). Posted
  `ops/team/messages/network-settings-n2/1788178312-barbara-liskov-recheck-claim.md`.
  Now exclusively own the serialized compiler, CTest, and private-bus lane
  until my terminal verdict releases it.
- 2026-08-31T06:24:16-06:00 — Midpoint material finding: all four former P2
  findings reproduce as repaired by direct execution, not by reading the repair
  handoff. Purpose-built harness against this tree's Debug artifacts shows the
  previously enabled-but-refused window now projects Scan/Connect/Disconnect as
  unavailable in the identical `Ready`/not-busy state, with every dispatch
  refused and the transport operation count unmoved, across both the
  post-operation and invalidation refresh windows. No QML references
  owner/epoch/revision and the wiki no longer claims it does; `bssid` is gone
  and no MAC-shaped value survives in any projected access-point or device row.
  Re-ran hostile probes against the committed `check_boundary.cmake`: all five
  evasions I rejected `43b563c` for are now rejected and a valid closed surface
  still passes. Recorded two residual guard-strength gaps as non-blocking — the
  gate admits a writable `Q_PROPERTY` and does not scan a `.qml` outside `qml/`.
  Posted
  `ops/team/messages/network-settings-n2/1788179056-barbara-liskov-recheck-midpoint.md`.
- 2026-08-31T06:27:01-06:00 — Terminally **ACCEPTED** exact
  `6f5d0ba9915851195a4776b3a1e2f224c369a958`, tree
  `38bfff6a125735166d4bec9d56fb134646d0ea12`, sole parent
  `43b563cdfe08455269375e2c356112902e562641`, with P0 0 / P1 0 / P2 0 / P3 3.
  Acceptance applies only to that exact SHA; I approved no prose and no other
  commit. Fresh strict Debug and Release configure and build from clean
  external roots exit 0 with zero warnings and zero errors; mutation-focused
  5-row proof 5/5, complete 14-row Network and Settings selector 14/14, and
  direct public-client package policy 5-row selector 5/5, each exit 0 in both
  profiles. Because the repair changes the shared public client, also swept
  `-L network` 25/25 and `-L settings` 9/9 exit 0 in both profiles.
  `tools/validate-docs` (112 documents), pinned strict MkDocs,
  `tools/check-source-shape` (three pre-existing unrelated warnings only), and
  `git diff --check` all exit 0. Repair diff bounded to 16 paths, +502/-66, with
  no forbidden path touched and no private service, libnm, D-Bus, host radio, or
  credential reach. Noted for the manager that `git merge-tree` against current
  main `ab203cac213b4bff882151de2398b4c1b46c99cb` conflicts additively in
  `docs/wiki/adr/index.md` and `mkdocs.yml` against the newer ADR-0053 rows, and
  that the repair edits the already-integrated, unowned Network N1 client module
  additively with its own focused test and architecture-page update. Posted
  `ops/team/messages/network-settings-n2/1788179221-barbara-liskov-recheck-verdict.md`
  and requested integration rather than a return to Radia Perlman; there is no
  blocking reproduction. Review worktree left detached, exact at `6f5d0ba`, and
  with tracked content byte-clean; I removed the `__pycache__` residue my own
  tool runs created and edited, amended, or committed no tracked candidate file.
  Handoff complete: the serialized compiler, CTest, and private-bus lane is
  terminal and released.
