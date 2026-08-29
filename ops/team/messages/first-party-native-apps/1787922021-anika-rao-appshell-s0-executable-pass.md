# Anika Rao AppShell S0 exact executable replay PASS

- Time: 2026-08-28T13:00:21Z
- Exact commit: `de52a04966763cc11f8a551c58bd76ca38694c5c`
- Tree: `c5a9e591314d4f3cd755a6595ca949f6ff0dc85c`
- Parent: `1b4e2846e40d31d79ffb03db2229c07ff9bca271`
- Worktree: clean
- Compiler/private lane after evidence: released to Devika Shah

Fresh Debug configuration with KWin, shell, and production shell disabled
passed. The manager-requested exact target build completed all 124/124 serial
actions. Qt generated and compiled
`qindaqt_app_shell_qmltyperegistrations.cpp` at action 99, proving the explicit
public coordinator registration repair.

Exact focused CTest selector `^qindaqt\.app-shell-` passed 5/5 in 4.43 seconds:

- `qindaqt.app-shell-action-registry` — PASS, 0.07 s;
- `qindaqt.app-shell-coordinator` — PASS, 0.03 s;
- `qindaqt.app-shell-surface-offscreen` — PASS, 0.18 s;
- `qindaqt.app-shell-source-policy` — PASS, 0.01 s;
- `qindaqt.app-shell-installed-consumer` — PASS, 4.12 s.

The final accessibility correction is therefore executable: the native window
title and item-derived application pane assertions both pass without the prior
invalid `ApplicationWindow` attachment warning. The installed row stages only
the exact AppShell/Tokens/Controls modules and AppShell headers, recompiles and
runs a downstream C++ consumer, clears ambient QML imports, and loads the staged
QML consumer.

No host desktop/session/input/configuration, service, compositor, or physical
device was touched. This proves the exact candidate executable at its focused
S0 boundary; Juno's independent exact source review and a reviewed
current-public-base merge candidate remain required before integration or any
product-progress change.
