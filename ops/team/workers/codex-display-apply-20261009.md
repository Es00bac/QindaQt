# Codex display Apply repair

- Status: working — diagnosing and repairing monitor positioning, scale, and rotation Apply failures
- Provider: OpenAI Codex
- Base: `5c69fcf81` (`origin/main`, refreshed from qinda)
- Branch: `fix/display-apply-20261009`
- Worktree: `.cache/display-apply-20261009`
- Owns: the smallest necessary display Settings/service/transaction/writer change, focused regression, and affected wiki documentation
- Acceptance: reproduce the Apply failure; verify positioning, scale, rotation and Keep/Revert through existing focused and private nested display gates; preserve the candidate on qinda

## Updates

- 2026-10-09T17:20:00Z — Claimed the user-requested minimal display Apply repair in an isolated worktree. Existing shared-checkout work is preserved. Inspecting the shared transaction path and live service diagnostics before selecting the fix.
- 2026-10-09T17:33:00Z — Reproduced live laptop Stage acceptance followed by Preview rejection `locked` with no monitor mutation. Lock1 reports unlocked. Production D6 still uses the legacy three-owner ScreenSaver quorum, while the native session owns the facades separately from the compositor. Switching only the private safety adapter to the existing native receipt monitor, authenticated by the writer's kernel peer PID.
