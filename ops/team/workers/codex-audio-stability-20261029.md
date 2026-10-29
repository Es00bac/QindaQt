# Audio stability investigation

- Role: Single-session implementer and integrator; no delegated model workers.
- Status: handoff — r44 recipe and archives published; exact source-build install command prepared.
- Base: `978b9b764` (freshly fetched hub main).
- Branch: `fix/audio-stability-20261029`.
- Ownership: Audio1 service/client, shell Audio applet, Settings Audio, the display/clipboard Wayland read callbacks implicated by the Settings core, focused tests and their documentation.
- Acceptance: causal reproduction, focused regression checks, strict documentation validation, preserved commit on qinda.

## Updates

- 2026-10-29T14:01:58-06:00 — Prepared r44 source `f0f875f5c`, overlay `d2dc0c1`, matching archives on both hosts and a one-package/zero-download laptop resolver plan. Actual Portage build/install awaits the user-run command; no background job remains. See `ops/team/messages/audio-stability/20261029-r44-ready.md`.

- 2026-10-29T13:38:11-06:00 — Source `08dedc68389546709e02eb50298e9e0cdb23bd6e` integrated on the hub. Acceptance and exact paths are in `ops/team/messages/audio-stability/20261029-handoff.md`. Available for the concrete Portage delivery follow-up; no unrelated queue or background worker was started.

- 2026-10-29T13:34:26-06:00 — Product and focused test targets compile under strict warnings. Ten distinct focused CTest groups pass, 79 QtTest results; the only initial failure was a service test reading the user's preset directory, repaired with explicit private store overrides and a passing 1/1 rerun. Buffer ASan/UBSan: 5/5 results. Display/clipboard pre-fix quiet-read binaries timed out; pre-fix meter visibility failed with a real private graph. Preparing the exact source commit and hub integration; no installation.

- 2026-10-29T13:23:00-06:00 — Latest Settings core shows a GUI-thread Wayland dispatch wait followed by external SIGABRT. Both private display/clipboard quiet-read regressions hang on pre-fix code (four-second cap, exit 137). Installed Audio1 exposes nine meter streams on the laptop, filling the applet's first eight application slots. Resuming the preserved, inactive endpoint recovery draft from qinda; adding bounded buffer and focus-lifetime repairs. No package or session mutation.

- 2026-10-29T13:14:19-06:00 — Claimed in an isolated worktree. Existing local and qinda changes are preserved. Both services are running without recent recorded Audio1/shell coredumps. Installed r42/r43 omit the October 21 Audio1 repairs; qinda's session log repeatedly reports invalid Settings Audio focus targets. Investigating source and installed behavior before changing delivery.
