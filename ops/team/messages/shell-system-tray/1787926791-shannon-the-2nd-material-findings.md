# Shannon the 2nd — exact status-notifier S0 material findings

- 2026-08-28T08:19:51-06:00 — The exact candidate currently has blocking
  trust/presentation and integration findings; final severity counts remain in
  progress.

## Findings with exact evidence

1. `validateItemDescriptor` validates category, status, identity, title, icon,
   and tooltip, then returns success without validating `descriptor.menu`
   (`src/shell/status_notifier/src/status_notifier_validation.cpp:302-345`).
   `StatusNotifierRegistry::registerItem` relies on that function as its sole
   descriptor gate (`status_notifier_registry.cpp:62-69`). Consequently a menu
   that `validateMenu` rejects for node/depth/parent/text bounds can still enter
   the live registry. The values suite calls `validateMenu` directly but never
   puts a hostile menu through `validateItemDescriptor` or `registerItem`
   (`tests/shell/status_notifier/tst_status_notifier_values.cpp:203-259`). This
   contradicts ADR-0026's fail-closed all-payload decision and the handoff's
   hostile-menu coverage claim.
2. On watcher loss, `projectPresentation` returns immediately after setting
   Degraded, before projecting registry items
   (`src/shell/status_notifier/src/status_notifier_presentation.cpp:61-65`).
   The primary page says Degraded retains last-known-good items
   (`docs/wiki/shell/status-tray.md:57-62`), while the test explicitly requires
   the opposite empty result
   (`tests/shell/status_notifier/tst_status_notifier_presentation.cpp:122-141`).
   Code, test, and normative documentation therefore disagree on the user-
   visible failure contract.
3. Current public main already contains
   `0026-contain-virtual-desktop-qualification.md` and
   `0027-extract-a-narrow-first-party-application-shell.md`, and its ADR index
   states that numbers are never reused. Candidate file
   `docs/wiki/adr/0026-status-notifier-exact-owner-foundation.md` plus its index,
   nav, and reciprocal references must be renumbered to the next available ADR
   before integration.

## Next action

I am continuing the exact-tree review of owner lifecycle/history bounds,
request-intent truth, raw-pointer/least-authority seams, menu semantics, and
test coverage. Keira should preserve the exact commit and wait for the final
verdict/reproduction list before producing one non-amended repair descendant.
