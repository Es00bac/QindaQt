# Expose selection repair — Codex

- Role: accountable implementer; manager integrates only after independent review
- Provider: OpenAI Codex; no independent model telemetry claim
- Status: working — checking the integrated source after signed Portage adoption
- Base: c05721d48388ce36512172ff6886f9ebd0ffea42
- Branch: fix/expose-selection-20261010
- Worktree: isolated expose-selection-20261010 worktree
- Ownership: Gather overview QML/controller/composition, focused Gather tests, primary Gather and testing wiki pages; this record and new expose-selection thread replies
- Acceptance: real pointer and touch selection dispatch exactly once without background dismissal; background and Escape still dismiss; focused Gather and relevant profile/resolution gates; strict docs and links; exact independent candidate review
- Boundaries: preserve unrelated work; no compositor restart or session teardown; manager owns integration and Portage delivery

## Updates

- 2026-10-10T17:17:25Z — Claimed the user-visible outcome from fetched qinda main in an isolated worktree. Investigating tile/background pointer-handler overlap and activation lifetime; no cause yet claimed.
- 2026-10-10T17:22:05Z — Strict focused build passed 311 actions under four-CPU/5GiB caps. Real Qt mouse input reproduced one activation plus one background dismissal (test exit 1). Expanding the regression to all three lanes and mouse/touch, with synchronous projection clearing as production does.
- 2026-10-10T17:28:02Z — All six old-QML mouse/touch cases reproduced simultaneous dismissal (exit 6); the three exclusive-grab changes pass all six at 100%, 125% and 150%. Gather/profiles/layout CTests pass 10/10 selected owning rows inside an 11-row run; the extra pre-existing panel-geometry row cannot load the static AgentUsageApplet plugin through external qmltestrunner. Strict docs and 551-document links/navigation pass. Preparing immutable candidate; no installed or live-desktop correction claimed yet.
- 2026-10-10T17:57:23Z — Exact source 2d99ca7a0 independently accepted at review4a9a31ff. Deployment42ec5bfa8 retains the R23 shell base from installed R28 and is pushed to qinda. Focused R29 overlay999c52c fully verifies the signed R28 image (2,622 paths); same reviewer checks resolved staging/trust findings before one frozen Portage build. Native frame inspection invalidated provisional corner-input results because the overview had not opened; correcting the applet trigger and requiring actual overview/target observation. No renderer changes: user withdrew the unrelated Vulkan comment.

- 2026-10-10T18:20:03Z — R29 Portage compile/install/package passed after adding the sole generated StartMenu staging plugin; exact overlay faa1f4c independently accepted. Full signature, embedded recipe and 2,622-path parity passed on qinda: only usr/bin/qindaqt-shell differs from signed R28. Private KGlobalAccel trigger actually maps Gather; old R28 ordinary mouse selects both fixture windows with correct active identity and top stack, so no native reproduction of the owner failure is claimed. A status-only live comparison and exact artifact mouse/touch replay are next.

- 2026-10-10T18:27:37Z — Signed R29 passed full signature, embedded exact recipe and all 2,622-path image parity on both hosts. Exact artifact mouse and touch replay each focused and raised two distinct fixture windows in a mapped private Gather reached through KGlobalAccel. Normal laptop Portage upgraded only R28 to R29; loaded replacement shell matches SHA cc61dd33, compositor owner and 19 other tracked process lifetimes remain unchanged, as do world/profile. Old R28 also passed native ordinary selection; physical owner failure remains unconfirmed, and the synthetic corner attempt did not qualify that trigger. Manager checks are next; no roadmap weight advances.
