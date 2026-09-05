# Andrea Ghez — claim: second bounded repair round (in-flight registration leak)

- Persona: **Andrea Ghez** (Z.AI GLM `zai-coding-plan/glm-5.3`, reasoning high).
- Claim time: 2026-09-04T19:54:52-06:00.
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/first-party-menu-export`,
  branch `worker/first-party-menu-export`.
- Exact base for this round: `7867177d` (handoff record on top of the rejected
  candidate `ba88f0b1`); product base remains `23b549db` / reviewed `e8e5170b`.
- Verdict read in full:
  `/home/cabewse/work_SPaC3/builds/qindaqt/lanes/review-menuexport-r2-codex/verdict.md`
  (Blackburn REJECT P1-01) together with its reproduction under
  `/home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/repros/inflight-registration`.

Finding accepted: `retirePublishedIdentity()` compensates only the
reply-confirmed `registeredWindowId`, so a registrar that accepted
`RegisterWindow(71)` but has not yet replied keeps a stale live 71 after
surface destruction and republication of 72. Plan for this round:

1. Track the attempted registration (window id + request serial) from the
   moment `RegisterWindow` is sent; every withdrawal path (surface destruction,
   accepted close, stop/quit, owner replacement) sends `UnregisterWindow` for
   the attempted id exactly once whether or not its reply arrived, keeps the
   exact-owner rule, and ignores a superseded serial's late reply.
2. New rows in `tests/app_shell/tst_application_menu_export_surface.cpp`:
   delayed-reply registrar + surface destruction requires
   `UnregisterWindow(71)` exactly once before `RegisterWindow(72)`, with the
   late 71 reply changing nothing; plus a rejecting-registrar control (no
   unregister needed, no crash). Both verified to fail on `ba88f0b1`.
3. Selector `ctest -R '^qindaqt\.(terminal|editor|text-editor|app-shell|global-menu-|file-manager)'`
   in Debug and Release plus the static gates.

Requested next action after handoff: independent exact review then manager
integration.
