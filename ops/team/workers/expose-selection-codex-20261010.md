# Expose selection repair — Codex

- Role: accountable implementer; manager integrates only after independent review
- Provider: OpenAI Codex; no independent model telemetry claim
- Status: working — preparing focused Portage delivery and exact source review
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
