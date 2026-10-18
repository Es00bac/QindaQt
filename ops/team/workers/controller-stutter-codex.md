# Controller frame timing Codex

- Role: Controller runtime incident implementer and integration owner
- Status: working — remove process inspection from compositor frame dispatch
- Base: `9b580024d45aa4476c1a79424e945108774d1552`
- Branch: `fix/controller-frame-stalls-20261017`
- Worktree: `.cache/controller-frame-stalls-20261017`
- Ownership: `src/controllers`, `tests/controllers`, controller architecture/ADR, incident documentation and bounded delivery recipe
- Execution: current Codex session only; no delegated agents or external model processes

## Updates

- 2026-10-18T00:47:44Z — Source/plugin build succeeds. Five controller CTest groups pass (28 QtTest cases, zero failure/skip); deliberately blocked observation proves GUI responsiveness, stale fencing and joined teardown. Strict MkDocs and 573-document local links pass. Same laptop proc-tree benchmark: original 426–503 ms, direct-link scan 129–133 ms; that remaining work now stays off the compositor. Preparing a bounded, signed r32 image from r31; installed/live frame acceptance remains pending.

- 2026-10-18T00:41:32Z — Measured twelve roughly 450 ms compositor CPU stalls at one-second intervals on qinda-top. DRM frame delivery skips 28–29 refreshes per stall; scheduler wait is at most 0.024 ms. A native stack identifies `inspectGamePriority` → `QFileInfo::symLinkTarget` from `ControllerRuntime::refreshPriority`. Implementing one coalesced observation worker, stale-result fencing and non-following FD reads; SDL/input remain on the compositor thread. Original shared checkout and unrelated changes preserved.
