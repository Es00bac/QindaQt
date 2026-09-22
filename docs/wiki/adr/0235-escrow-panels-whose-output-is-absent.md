# ADR-0235: Escrow panels whose output is absent instead of failing the editor

- **Status:** Accepted
- **Date:** 2026-09-22
- **Owners:** shell_customization, shell_customization_editor
- **Supersedes:** None
- **Superseded by:** None

## Context

`LayoutEditingRepository` validates its initial profile with
`LayoutCandidateValidator`, which runs `PanelLayoutSolver::solve()` over every
panel. The solver fails with `MissingOutput` when a panel names an output the
current generation does not have. A failed initial validation leaves the
repository non-ready with no snapshot, and every coordinator command is then
refused.

Both editor compositions sit on that repository: the shell's `LiveEditorHost`
and the Settings Customize route's `RepositoryCustomizeEditorHost`. They are
held byte-identical by the parity invariant in
[Customization editor](../shell/customization-editor.md).

The consequence was a dead user interface. `LiveCustomizationController` gates
every Meta+right-click entry point on `available()`, which requires
`LiveEditorHost::ready()`. One panel pinned to an unplugged or reconfiguring
display therefore removed in-place customization entirely — no panel menu, no
applet menu, no desktop menu — until an unrelated layout-profile adoption
happened to rebuild the host:

```
QindaQt shell live customization is unavailable: panel 'dock' names missing output 'eDP-1'
```

ADR-0234's work made this reachable in normal use, because the dock is pinned
to `eDP-1`. The same failure closed the Settings Customize route.

`PanelProfileOutputOccupancy::presentOutputsOnly()` already solved the
equivalent problem for the runtime surface path: filter the profile to the
panels this generation can host, so an absent output cannot void the whole
solve. Applying that filter to the editor is *not* sufficient, and applying it
naively is worse than the defect. The editor persists what it edits:
`EditorSession::applyToUserProfile()` writes the session's profile to
`<userProfileDirectory>/<id>.json`, which is the only stored record of the
user's layout. An editor handed a filtered profile would write that filtered
profile back, **permanently deleting every panel on whichever displays happened
to be absent during the edit.** Customizing a panel on an external monitor with
the laptop lid shut would silently erase the dock.

The two requirements are in tension only if "not editable" and "not stored" are
the same state. They are not.

Alternatives considered and rejected:

- *Relax the solver or the candidate validator for absent outputs.* This is the
  same rule that refuses a user who picks a disconnected display for a panel.
  Relaxing it would accept that edit and produce a panel nobody can see.
- *Merge the absent panels back in `LiveCustomizationController`.* The write
  happens inside `EditorSession`, below the controller, so the controller
  cannot reach it — and the Settings route would stay broken.
- *Make `committedProfile()` the merged profile.* The coordinator's undo stack
  is built from `committedProfile`, so merged entries would re-enter candidate
  validation and fail on the absent output.

## Decision

A panel the current output generation cannot host is **escrowed**: carried by
the repository, excluded from the session, and re-attached at the persistence
boundary.

- `LayoutEditingRepository` partitions its initial profile once, at
  construction, using `PanelLayoutSolver::outputsCanHost()`. Panels that fail
  become `EscrowedPanel` records holding the panel and its stored index.
- `LayoutEditingSnapshot` covers only hostable panels. Its invariant is now
  "the successful solve over every panel the generation can host"; every panel
  in a snapshot still has a layout entry, so snapshot consumers are unchanged.
- Escrow is **fixed at construction and never extended by candidate
  validation**. An edit that moves a panel onto an absent output is still
  refused with `MissingOutput`, because that panel is in the candidate.
- Every write of an edited profile to durable storage goes through
  `LayoutEditingRepository::withEscrowedPanels()`, which restores escrowed
  panels at their stored indices. `EditorSession::applyToUserProfile()` is that
  single boundary.
- `PanelLayoutSolver::outputsCanHost()` is the one definition of the match.
  `PanelProfileOutputOccupancy::presentOutputsOnly()` delegates to it, so the
  runtime filter and the editor partition cannot drift apart.

Escrowed panels return to the session on the rebuild that follows their
display's return, which is the existing output-generation path.

## Consequences

- In-place customization and the Settings Customize route both survive a
  display being unplugged or reconfigured. The fix is in the shared repository,
  so the parity invariant holds without a second change.
- A panel on an absent display is not editable while that display is away. This
  is deliberate: it has no geometry to edit against. It is visible to neither
  `panelIds()` nor the panel menu, and returns untouched.
- Apply is no longer lossy across a hotplug. This is the obligation the ADR
  exists to record: **a new persistence path for edited profiles must merge the
  escrow.** Saving a session profile directly reintroduces silent data loss.
- Stored panel order is preserved across an edit made while a display is
  absent, because escrow restores by index rather than appending.
- Covered by `qindaqt.customize-editor-live-host`
  (`staysReadyWhenAPinnedOutputIsAbsent`, `keepsAbsentOutputPanelsWhenApplying`)
  and `qindaqt.shell-live-customization-controller`
  (`survivesAPinnedDisplayGoingAway`), which asserts the full unplug → edit →
  apply → replug cycle including the stored order.

## Revisit when

The editor gains a way to edit a panel against a remembered or virtual output
geometry, which would make an absent-output panel editable and retire the
"carried but not edited" state. Also revisit if a second durable write path for
edited profiles appears, since the merge obligation above would then need to be
enforced by type rather than by review.
