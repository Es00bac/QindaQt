# ACCEPT 58d1e3385b35351b5510a4e4af72a7f085d5f761

- Time: 2026-09-27T01:20:36.389670+00:00
- Reviewer: private-session-isolation-review
- Candidate: 58d1e3385b35351b5510a4e4af72a7f085d5f761
- Resolves rejected candidate: c0e976cb27b987746a6fa71cecaf9544eb04437a
- Reviewer worktree: container-wm-review-private

## Verdict

Accept for integration. No remaining blocking findings in the assigned session activation boundary. The earlier moving-code shared-broker publication gap is closed by the private-scope early return before any broker or manager call. The exact-candidate physical KWin compatibility blocker is resolved by bounded paired comm/argv0 fallback when the executable link is inaccessible; available conflicting executable evidence is never overridden.

## Independent evidence

- Fresh reviewer build root configured Release, KWin plugin OFF, production shell/session ON; built activation-environment, resident-service-refresh, and activation-policy targets at exact candidate, exit 0.
- `ctest --test-dir build/review-isolation --output-on-failure -R 'qindaqt.session-(activation-policy|activation-environment|resident-service-refresh)$'`: exit 0, 3/3 pass, 3.04 seconds.
- A separate read-only helper compiled against exact candidate policy called `witnessedSessionActivationScope(1413957)` on the actual physical DRM KWin, returning `physical`, exit 0. The same helper on rejected candidate returned `private`. Installed KWin has cap_sys_nice and hides its executable link; comm/status/cmdline remain readable.
- `git diff --check d3b646f2..58d1e338`: exit 0.
- Reviewed ADR-0276, compositor/session architecture, testing harness changes, helper interfaces, production call wiring, native sd-bus preservation, private defaults, and hermetic fake endpoint tests.

## Boundaries and next action

These tests do not restart physical services or prove microphone-to-text delivery. Source review plus hermetic tests prove the activation guard; the additional live probe proves privileged-compositor classification only. Private harnesses needing activation now own their broker setup, as documented. This is protection from accidental cross-session interference, not a hostile same-user security boundary.

Manager should integrate exact candidate, rerun affected gates, preserve staged RUNPATH when deploying qindaqt-session, and leave the active physical compositor/session running. The prevention applies to future supervisor startups. Reviewer is available for bounded integrated-gate or deployment findings; no unrelated outcome claimed.
