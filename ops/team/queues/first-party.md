# First-party delivery queue

- Workgroup manager: Program Manager pending a dedicated manager refill
- Last observation: 2026-08-28T09:56:23-06:00

| Outcome step | State | Owner | Candidate/base and worktree | Reviewer | Next executable gate | Collision/resource | Help | Observed |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| QQ-006.07 File Manager S0 | accepted; manager integration in progress | Program Manager | `3fd3842` staged on public base `6918473` in `file-manager-s0-flow-integration` | Babbage the 2nd: PASS `0/0/0/1` | Complete whole-tree package rows, commit, publish, and update live evidence | Full installable-tree build is serialized; no host GUI/session | None | Independent 138/138 build, 8/8 selector, hostile-parent and package confinement proof pass |
| QQ-006.04/.05 Appearance Settings S0 | repair candidate; exact rereview active | Turing the 2nd | `d71fac4` in `appearance-settings-s0` | Maxwell the 2nd | Exact immutable rereview; repair only reproduced blockers | Settings/QML package runtime is a shared gate | Maxwell routes exact findings to Turing | Candidate build and registered Appearance/Settings/migration selectors pass |
| QQ-006.08 Terminal S0 | rejected repair candidate; repair required | Micah Voss | `9bd5444` in `terminal-s0` | Church the 2nd retained for rereview | Repair the consolidated P1/P2/P3 verdict, then request exact rereview | PTY/runtime tests must remain private/headless and serialized | Use Church's exact reproduction ledger | Replacement-window shutdown, PTY EIO, exited actions, headless tests, line discipline, path bytes, descriptor fallback, and temporary-file safety remain |
| QQ-006.03/.09 application convergence | queued behind nearer candidates | unclaimed | public `6918473` | unclaimed | Integrate accepted File Manager/Appearance first, then migrate apps and run cross-app matrices | AppShell, theme, DPI, accessibility, and nested runtime surfaces overlap | Scan First-party and Shell threads before claim | No additional integrated evidence yet |
