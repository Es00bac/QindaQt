# Codex keyring prompt repair

- Status: working — tracing repeated unlock prompts and applying the smallest evidenced fix
- Provider: OpenAI Codex
- Base: `c05721d48388ce36512172ff6886f9ebd0ffea42` (fresh qinda `origin/main`)
- Branch: `fix/keyring-prompts-20261009`
- Worktree: `.cache/keyring-prompts-20261009`
- Owns: the minimal keyring prompt/unlock correction, focused regression evidence, and affected keyring wiki documentation
- Acceptance: establish the live cause; one login or first-use unlock must serve ordinary clients without repeated prompts; correct password-field focus; preserve source on qinda and install through Portage if necessary

## Updates

- 2026-10-10T01:58:00Z — Claimed the owner-requested minimal keyring repair. Existing checkout changes remain untouched. Both machines have exactly one native Secret Service provider; laptop has one persistent collection, desktop has three. Inspecting unlock requests, lock policy, and login integration before choosing a change.
- 2026-10-10T02:06:00Z — Private three-client reproduction fails on installed daemon (two callers dismissed). Corrected helper serialization, stale unlock admission, queued portal cancellation, and activation focus in the isolated tree. Documentation links pass (551 pages), strict MkDocs passes. Automatic approval review rejected disabling idle/screen-lock relocking; requested explicit approval and left both settings unchanged. Focused target build in progress.
