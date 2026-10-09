# ACCEPT — display Apply candidate 927039197bbfaae44ddadf1d5e22e47eb7d1c04f

- Reviewer: codex-display-apply-review-20261009
- Candidate: `927039197bbfaae44ddadf1d5e22e47eb7d1c04f`
- Base: `5c69fcf81`
- Worktree: `.cache/display-apply-20261009`
- Scope: exact source review, read-only product paths; worker record/reply only are authored here

## Updates

- 2026-10-09T17:38:55Z — Claimed the requested independent review without duplicate builds or private fixtures.
- 2026-10-09T17:40:29Z — ACCEPT; no blocking defects found in the minimal adapter switch.

The existing native transport obtains the compositor service's exact unique owner and its PID from the bus daemon. Its subscription and nonce-correlated targeted receipts reject other senders and stale owner/generation/serial work. The new admission callback matches that daemon PID only to the independently obtained writer `SO_PEERCRED` PID. The resident still requires live writer authority, an authenticated unlocked snapshot, a held logind delay descriptor, and no pending suspend before admitting a display mutation. Locked/Locking remain Locked; missing or denied native state remains Unknown. The logind, sleep, teardown, writer and transaction paths are unchanged.

The adapted existing private-bus regression supplies NativeLock1 with a direct `QDBusContext` backend and keeps the compatibility facade names on a distinct bus owner. It retains wrong expected PID, Locking/Locked/Unlocked, delay arguments/lifetime, suspend/resume and logind owner-loss assertions. CMake supplies the constants' owning library.

Reviewed paths: `src/services/display_runtime/src/qt_session_safety_port.cpp`, `tests/services/display_runtime/{CMakeLists.txt,tst_session_safety_private_bus.cpp}`, and `docs/wiki/{architecture/display-service.md,apps/display-settings.md}`; also read the unchanged native monitor/transport, writer peer acquisition, resident safety composition, ADR-0304 and ADR-0314.

Verification: `git diff --check 5c69fcf81 927039197bbfaae44ddadf1d5e22e47eb7d1c04f` exited 0. No builds or tests were run by the reviewer; focused execution is manager-owned and still required. This source verdict makes no installed/hardware/UI convergence claim.

Requested next action: run the already active focused gate, then integrate and verify the same candidate. After reading the Platform queue, the reviewer offers an exact reproduction/repair recheck if the gate reveals a blocker; no additional implementation is claimed.
