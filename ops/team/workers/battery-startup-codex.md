# Battery Startup Codex

- Role: Platform battery-startup implementer
- Provider: OpenAI Codex
- Status: available — Verified battery startup candidate ready for independent review
- Assignment: Battery readings after fresh login with installed dormant UPower
- Candidate: `6921f4c218550aa6e8f7690a909d3e2d633b603b`
- Base: `46e6a74dc0de6b279ca8c2a24e50d3f86634d925`
- Branch: `worker/battery-startup-20261007`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-battery-startup-20261007`
- Ownership: UPower adapter/header, focused UPower/production tests and registration, power-service wiki page, own board/messages
- Next gate: Independent review of the immutable battery-startup candidate, then manager integration/package gates

## Updates

- 2026-10-07T14:37:54Z — Claimed assigned isolated qinda worktree at exact base; clean tree verified. Existing main checkout untouched. The adapter currently resolves a missing owner synchronously and never requests activation.

- 2026-10-07T14:44:43Z — Implemented bounded asynchronous owner resolution and dormant-service activation while retaining refresh/generation fences. Added private activation fixture and seven focused production/failure/lifetime/competing-owner behaviors. Dedicated strict Debug configuration on qinda uses -j2 and the installed fork-compatible /usr/libexec configure path; no installation occurs.

- 2026-10-07T14:54:21Z — Verification complete: strict Debug eight-target build exits 0; nine focused/adjacent CTests pass, 96 Qt checks, 0 skipped, 10.86 seconds. Seven new startup behaviors pass (9/9 Qt checks). The unchanged base adapter fails the dormant regression with zero supplies (exit 1); fixed source restored/rebuilt and full cohort passes. Strict MkDocs exits 0; direct repository checker validates 510 Markdown documents; git diff check exits 0. Direct process inspection finds zero remaining startup fixture processes. Available for the same repair/review loop; no installation or physical startup acceptance claimed.

- 2026-10-07T14:55:47Z — Handed off exact candidate 6921f4c218550aa6e8f7690a909d3e2d633b603b for independent review. Read Platform queue and current incident peer threads in manager worktree; offered bounded same-worktree repair/private-activation support for any exact blocking reproduction. Compiler and private-bus resources are released. Candidate is already preserved by the hub worker branch; the following self-owned receipt changes only the board.
