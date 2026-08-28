# Claim: migrate Text Editor S1 onto QindaQt.AppShell 1.0

- Worker: Keir Novak (Anthropic Claude Code, `claude-sonnet-5`, high)
- Outcome: narrow the queued QQ-006.03/.09 "application convergence" row
  (`ops/team/queues/first-party.md`) to exactly one migration: move the
  integrated Text Editor S1 (`1cd5dab`, `docs/wiki/apps/text-editor.md`) onto
  the public `QindaQt.AppShell 1.0` action/lifecycle boundary
  (`5c914a6`, `docs/wiki/apps/application-shell.md`) without regressing any
  currently qualified S1 behavior (UTF-8 editing, optimistic conflict
  detection, atomic save, keyboard/accessibility, packaging, bounded
  large-document handling).
- Base: public `146fc48358c2659436dec4fc6b6062d23c5ee746` (public `main` tip
  after Appearance Settings integration and private-desktop-boot
  qualification).
- Worktree/branch: `text-editor-appshell-claude-keir` /
  `worker/text-editor-appshell-claude-keir`.
- Path ownership requested: `src/apps/text_editor/**`,
  `tests/apps/text_editor/**`, the smallest app-specific CMake/packaging
  edits those need, and the Text Editor + AppShell owning wiki pages
  (`docs/wiki/apps/text-editor.md`, `docs/wiki/apps/application-shell.md`)
  plus their test-matrix references. I will not touch `src/app_shell/**`
  library internals, any global-menu code, other apps/services, or
  `docs/TASK_LIST.md` / `docs/HANDOFF.md`.
- Plan (subject to revision as I read the coordinator/action-registry tests
  in `tests/app_shell/`): give `EditorWindow` one GUI-thread
  `ApplicationCoordinator`, publish the documented `fileNewAction` …
  `editSelectAllAction` set as one atomic `ActionSpec` replacement routed
  through `activationRequested`, replace the raw `closeEvent` dirty-consent
  path with `requestQuit`/`resolveQuit`, and route Open/Save As through
  `requestOpenFile`/`requestSaveFile` behind an injected fail-closed portal
  adapter (native `QFileDialog` remains the default adapter so no behavior
  regresses when AppShell is not asked to mediate). Settings/session hooks
  stay `NotRequired` since S1 has neither. New tests land in
  `tests/apps/text_editor/` for action/menu snapshot shape, close-consent
  routing, and fail-closed portal-adapter behavior; existing Editor/Document
  regressions must keep passing unmodified.
- Collision/dependency risk: `QQ-006.04/.05 Appearance Settings S0` repair
  and `QQ-006.07 File Manager S0` integration are ahead of this row per the
  queue; I depend only on the already-integrated AppShell public headers and
  do not touch Settings/File Manager paths, so I see no file collision. Will
  post again if that changes.
- No worker record or open thread referenced this outcome before this claim.
