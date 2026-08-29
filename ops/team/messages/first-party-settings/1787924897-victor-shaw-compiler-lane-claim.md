# Compiler-lane claim: Appearance Settings S0 focused gates

- **Timestamp:** 2026-08-28T13:48:17Z
- **From:** Victor Shaw, Appearance Settings S0 implementer
- **Authority:** manager compiler-lane release for this outcome
- **Candidate tip:** `ef19a9b` on `worker/appearance-settings-s0` (base
  `9db68c4`; clean tree)

Claiming the released serial compiler lane now. Scope, per the release
instructions and my source-static handoff
(`1787924691-victor-shaw-source-static-handoff.md`):

1. Configure the `dev` preset once (serial, no whole-tree build).
2. Build only the focused targets my handoff names:
   `qindaqt_appearance_{values,preview,settings-model,page}_tests`,
   `qindaqt-settings`, plus their transitive QML/plugin dependencies.
3. Run exactly: `qindaqt.appearance-{values,preview,settings-model,page}`,
   `qindaqt.settings-app-offscreen`,
   `qindaqt.settings-app-rejects-unknown-route`, and the new
   `qindaqt.settings-app-desktop-identity` gate (source contract + compiled
   binary embedding + desktop-entry Exec + install registration — the
   manager/Rhea identity repair evidence).
4. Documentation/source/static gates after any repair.

Identity direction acknowledged and already implemented: the tip adds
`setDesktopFileName(QStringLiteral("org.qindaqt.Settings"))` before engine
creation in `src/apps/settings_center/main.cpp` plus the focused
`check_desktop_identity.cmake` contract test requested by Rhea Calder
(`1787923020-rhea-calder-settings-desktop-identity-request.md`) and the
manager correction. No Mina-specific message was present in the thread at
claim time.

Offscreen only; no GUI, session, input, display, or host configuration.
Repairs will be non-amended descendant commits. Terminal state: handoff +
explicit lane release on this board.
