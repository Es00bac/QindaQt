# ADR-0122: Adopt saved layout preferences live through surface reconciliation

- **Status:** Accepted
- **Date:** 2026-09-10
- **Owners:** Shell runtime, Settings Customize route
- **Supersedes:** the "live shell binding stays future work" clause of the
  Customize route stopping point
- **Superseded by:** None

## Context

The Customize route's Apply already persists two truths: edited profile
content into the user profile store (which merges over the installed catalog,
last-writer-wins by profile id) and the selection into Settings1
`panels.layoutProfile` (ADR-0074). But the shell resolved both exactly once,
before the initial surface plan: the appearance bridge applied theme, fonts,
and accessibility from every confirmed snapshot while explicitly ignoring the
layout key, and nothing watched the user store. Applying a layout therefore
did nothing a user could see until the next session — reported as "Apply
layout does not apply the layout".

Restarting the shell process to adopt a layout was rejected: the paced
recovery budget (ADR-0103) exists for failures, not deliberate changes, the
supervisor exposes no restart API, and ADR-0019's replacement argv is a
crash-recovery path whose budget must not be spent by ordinary settings use.

## Decision

1. **One adoption path: reconciliation.** A confirmed Settings1 snapshot that
   names a different `panels.layoutProfile` (surfaced by the appearance
   bridge after the initial baseline latch) and user-store writes (watched
   directory and per-profile files, debounced) both trigger the same
   adoption: reload the profile catalog from the directories captured at
   startup, re-select explicitly (the loader resets the current index),
   refresh the visibility runtime's hide-mode inventory from the adopted
   profile, and run the existing debounced `reconcileSurfaces`/
   `settlePanelVisibility` cycle. `PanelSurfaceController::reconcilePlan`
   already reconfigures the live surface set incrementally — the same path
   output hotplug uses — so no shell restart, compositor restart, or
   application disruption occurs.
2. **Startup precedence and failure semantics carry over unchanged.** An
   explicit `--profile` outranks the saved selection for the process
   lifetime (adoption is skipped with a diagnostic); an unknown saved id
   fails closed to the prior selection; a failed catalog reload leaves the
   catalog and the running layout untouched. The user store still
   participates last in the directory merge, so an edited copy of a built-in
   profile wins on reload exactly as at startup.
3. **Ownership stays put.** The route keeps persisting content-then-selection
   (its atomicity contract is unchanged); the shell owns when and how the
   running desktop changes. Wallpaper and theme remain appearance-owned and
   unaffected by layout adoption.

## Consequences

- Apply now visibly changes the running desktop: a saved layout switch or an
  edited panel set is adopted within one reconcile cycle, with the panels'
  own visibility rules re-derived from the adopted profile.
- Repeated Applies are safe: adoption is idempotent, debounced (250 ms for
  store writes), and reconciles nothing when the resolved plan is unchanged.
- The user-store watch must be refreshed after each adoption (files are
  replaced on save); a store directory that appears only with the first
  Apply is covered by watching its parent until then.
- Coverage: the catalog adoption policy (content reload, selection switch,
  fail-closed, CLI lock, reload failure) is proven by
  `qindaqt.shell-runtime-layout-adoption`; the bridge's latch-and-emit
  behavior by `qindaqt.shell-runtime-token-publication`; hide-mode refresh
  rides construction parity in the visibility producer rows.
