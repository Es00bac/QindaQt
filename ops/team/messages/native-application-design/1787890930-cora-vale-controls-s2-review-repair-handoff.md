# Controls S2 exact-review repair candidate handoff

- Author: Cora Vale
- Time: 2026-08-28T04:22:10Z
- Commit: `5be6df91b8aa2a06fc5c07bef44d39857094e088`
- Tree: `000e58c658f8d17e896d2b88a7c1266bc8d5831c`
- Direct parent: `10996f146ff78f69a6f1019933d812d1475faf85`
- Branch/worktree: `worker/controls-s2` at
  `/home/cabewse/work_SPaC3/container-wm-workers/controls-s2`

## Repaired outcome

This one non-amended descendant closes both P2 findings from Tessa Rowan's
exact review and all three local P3s:

- Qt's queried QML deploy paths install all 14 public source documents. The
  clean stage rejects missing/extra QML, runs strict bare-path qmllint against
  representative public properties, runs the compiled consumer, and retains
  the Controls-to-Tokens relative RUNPATH.
- StateCard readiness is private. Status/title/message mutations in one event
  turn publish exactly one latest complete tuple with settled politeness;
  construction is silent. The behavior gate covers all five statuses,
  same-status Warning/Error content, status-before-content order, no synchronous
  stale tuple, and no duplicate event.
- Production QML imports are positively allowlisted; visual-row comments name
  process isolation and QST pixel settlement separately; both named Noto host
  families are required and docs no longer claim byte-pinned fonts.

Changed paths are confined to Controls source/build rules, Controls tests and
fixtures, `docs/wiki/shell/controls.md`, and the smallest additive Controls
sections of `docs/wiki/development/testing-harness.md`. The StateCard assertions
live in a cohesive helper, keeping the existing behavior file below the
decomposition threshold. No shell/service/display source changed.

## Exact evidence

- Debug configure exit 0; focused affected build plus Controls qmllint 24/24,
  exit 0; exact Controls selector 29/29, exit 0.
- Release configure exit 0; focused affected build plus Controls qmllint 30/30,
  exit 0; exact Controls selector 29/29, exit 0.
- Both selectors include behavior, 25 reviewed unchanged visual comparisons,
  source policy, PSS, and installed package. Both named font assertions execute
  in every visual row.
- Debug package: exact 14 QML, strict tooling and compiled runtime passed;
  direct Tokens dependency and `RUNPATH [$ORIGIN/../Tokens]` witnessed.
- Release package: the same exact 14/tooling/runtime/RUNPATH evidence passed.
- Debug PSS median: bare 16,465 KiB, Controls 36,513 KiB, delta 20,083 KiB;
  Release: bare 16,446 KiB, Controls 35,915 KiB, delta 19,486 KiB. Threshold is
  explicitly null in both.
- `tools/validate-docs`: 46 documents/nav, exit 0; strict MkDocs 1.6.1 offline
  build exit 0; source shape 820 files/zero allowlisted, exit 0; direct 14-file
  source policy and `git diff --check` exit 0; Debug/Release discovery each
  exactly 29.
- `git show --check 5be6df9`: exit 0. Worktree clean. Compiler temp
  `build/controls-compiler-tmp.JAidNM` and private runtime root
  `/home/cabewse/.cache/qc.8jMxqe` were empty, removed by exact path, and are
  verified absent; no candidate process remains.

## Caveat and requested next action

No broad registry pass is claimed. The rejected parent already recorded the
unrelated reproducible `shell.production-surface.1080p` timeout; this repair
does not touch that owner boundary and proportionally reran only Controls-owned
gates. Please have Tessa Rowan rereview exact commit
`5be6df91b8aa2a06fc5c07bef44d39857094e088`, specifically attacking staged
tooling/source inventory, event-turn announcement ordering/encapsulation, and
the three P3 truth repairs. Integrate only after that exact rereview passes.
