# Current integrated documentation and compositor ABI gates

Exact documentation integration `14ded57a3c312039f4da97a39c19b2b3affb4237`
is preserved in qinda's hub and fast-forwarded into the manager's clean isolated
desktop worktree. These are additional integrated gates, not worker activity
or a full program-completion claim.

| Gate | Actual result | Process and cleanup |
| --- | --- | --- |
| Link/navigation validator | exit 0, 489 Markdown documents, 0.468 seconds | PID 603235 / start tick 36574805 gone; frozen hashes unchanged |
| `mkdocs build --strict` | exit 0, 8.014 seconds (MkDocs reports 7.67) | PID 603242 / start tick 36574854 gone; frozen hashes unchanged; minimum available memory 13,621,184 kB |
| Selected fork ABI and conflicting-stock poison | 2/2 CTests, exit 0, 0.109 seconds | PID 604356 / start tick 36591743 gone; ABI manifest/module/cache unchanged |

Both documentation commands and the ABI runner have zero core limits, no scoped
cores and no observed surviving descendants. The ABI poison genuinely rejects
stock `6.6.6` when exact fork ABI `6.6.6.1` is required. This is the source/CMake
version guard; it does not stand in for dynamic consumer plugin qualification.
Raw command/input-hash/status logs remain in qinda's manager ignored build roots
`manager-current-docs-14ded/` and `manager-integrated-abi-20261002/`.

Current screenshot gallery is still visible in isolated QindaFox session
2935743 / tab 1 at `http://127.0.0.1:8765/`; a fresh API and rendered page show
44 current captures, latest commander-t64, with current numbering and the
five-second live refresh. The gallery serves existing game screenshots only.
No laptop build or GPU work was performed.

Current resource routing: lid reviewer owns the sole private runtime lease;
helper-identity author owns the sole compiler for a standard fresh Release,
capture-test-authorization-enabled two-target dependency build. Copying the
manager's absolute-path CMake cache was rejected as an unsafe shortcut; its
production and native frozen images remain unchanged. Ordinary-property owner
preserved the one diagnostic failure and has only a source logging continuation,
with no compiler/runtime lease. Installed routes, package/plugin/privacy gates
and full PF2/PF19 remain open; no feature-ledger advancement follows.
