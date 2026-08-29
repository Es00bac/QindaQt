# QindaQt direct-manipulation shell customization — architecture and acceptance matrix

- Posted: 2026-08-28T13:11:01Z (unix 1787922661)
- Author: Liora Vale — Anthropic Claude Opus 5 (`claude-opus-5`), reasoning maximum
- Kind: architecture and planning. **No code, build, test execution, UI/session launch, or qualification is claimed by this document.**
- Exact read-only base: `9db68c4023257b49421101fa1b13c73bbc2cfa85` (public `main`)
- Product step this serves: `QQ-004.08` "Direct WYSIWYG customization and reveal affordances" (weight 11, `MODELLED`), with reach into `QQ-004.02` and `QQ-004.09` (`ops/team/features.json`)

## 0. How to read this

Every statement carries one tag:

- **[E]** exists at the base commit; a path and line is cited and can be re-read.
- **[P]** proposed by me; it does not exist and is not accepted until the named ADR/wiki step lands.
- **[M]** missing; required work with no implementation today.

Acceptance commands are written for an implementer to run. I ran none of them.
No percentage of completion is implied anywhere.

---

## 1. Grounded starting position

| Layer | State | Authority |
| --- | --- | --- |
| Profile schema v1 (strict, atomic, structured errors) | **[E]** | `docs/wiki/reference/profile-schema-v1.md`; `src/profiles/include/qindaqt/profiles/layout_profile.h:15-61` |
| Pure panel geometry solver, wildcard expansion, work areas | **[E]** | `src/shell_layout/include/qindaqt/shell_layout/panel_layout_types.h:22-72`; `docs/wiki/shell/layout-profiles.md:59-98` |
| Transactional editing: leases, snapshots, optimistic revisions, undo/redo, preview, `evaluate()` | **[E]** | `src/shell_customization/include/qindaqt/shell_customization/{editing_commands.h:15-134,layout_editing_coordinator.h:24-35,layout_editing_repository.h:36-77}` |
| Manifest-aware placement policy at edit time | **[E]** | `src/shell_customization/src/applet_placement_validator.cpp:95-174`; `docs/wiki/reference/applet-manifest-schema-v1.md:75-92` |
| Window-aware hide policy (`never/always/dodge-*/maximized/intelligent`) + reservation intent | **[E]** | `src/shell_visibility/include/qindaqt/shell_visibility/panel_visibility_types.h:107-139`; `docs/wiki/shell/panel-visibility.md:33-60` |
| Reveal/hold move-only lease store | **[E]** | `src/shell_orchestration/include/qindaqt/shell_orchestration/panel_interaction_store.h:25-75` |
| Real LayerShellQt panel surfaces, in-place transitions, hotplug reconcile | **[E]** | `docs/wiki/shell/panel-surfaces.md:9-92`; ADR-0007 |
| QST-1 tokens (pure, `toVariantMap()`) and `QindaQt.Controls 1.0` | **[E]** | `src/design_tokens/include/qindaqt/design_tokens/token_deriver.h:42-44`; `docs/wiki/shell/controls.md` |
| `qindaqt-settings` executable + Settings1 client | **[E]** | `src/apps/settings_center/CMakeLists.txt:2-13` |
| **Production edge reveal, pointer containment, hide animation, shortcut reveal producers** | **[M]** | `docs/wiki/shell/panel-visibility.md:82-86` |
| **Any customization UI, drag palette, canvas, drop targets** | **[M]** | `docs/wiki/shell/layout-profiles.md:165-171` |
| **Shell subscription to provisional editor snapshots** | **[M]** | same |
| **User-profile persistence** | **[M]** | `src/profiles/include/qindaqt/profiles/profile_catalog.h:25`; `src/shell/runtime/runtimeoptions.h:17` |
| **Multi-row applet flow, floating margins, opacity, desktop container** | **[M]** | `src/shell_layout/src/panel_layout_solver.cpp:80-85`; `docs/wiki/reference/profile-schema-v1.md:117-119` |

Two consequences that shape everything below:

1. The mutation model is **done**. This lane builds presentation, transport,
   persistence and reveal — not a second edit engine.
2. Two shipped profiles (`minimal`, `xfce-inspired`) contain a `hideMode: always`
   panel and **no reveal path exists**, so those panels are currently
   unreachable in a live session. That is a live product defect independent of
   the editor.

---

## 2. P0 decisions — required before any customization code

These are the decisions without which an implementer must guess. Each states
the decision, the reason, and the rejected alternative.

### D1 — The manifest catalog must cover every plugin ID the built-in profiles instantiate

**Decision [P].** Before the editor ships, `data/applets/` gains a manifest for
each of the 21 plugin IDs referenced by `data/profiles/*.json` that has none,
and a test asserts the uncovered set is empty. The manifests declare real
`placements`/`sizing` and an honest `entryPoint`; runtime resolution still
reports `implementation-unavailable` for the ones with no compiled
implementation, which is already the modelled honest state
(`docs/wiki/shell/applet-runtime.md:72-76`).

**Reason.** `AppletPlacementValidator::validatePlacement()` returns
`ManifestUnavailable` for any plugin with no manifest
(`src/shell_customization/src/applet_placement_validator.cpp:99-106`), and
`placementChanges()`/`zonePlacementChanges()` (`:149-174`) require a manifest
decision whenever a drag changes orientation or zone. **34 of the 63 applet
instances in the ten stock profiles (54%) have no manifest.** Without D1, the
majority of first drags a user attempts in a stock profile fail with a typed
error. The editor would be technically correct and practically unusable.

**Rejected.** Relaxing the validator to allow unmanifested zone changes —
that would silently grant a compatibility claim the package never made and
would contradict `docs/wiki/reference/applet-manifest-schema-v1.md:84-92`.

### D2 — One pointer gesture is one preview bracket

**Decision [P].** `BeginPreview` opens on drag threshold, provisional commands
run during the drag, `CommitPreview` closes on drop, `CancelPreview` closes on
Escape/invalid release. There is **no separate user-facing "live preview mode"**
and no F5/F6/F7 preview triad.

**Reason.** A zone-crossing drop is inherently two commands — `MoveApplet` for
order plus `UpdateAppletSettings` for `settings.zone` — because applet order is
one flat list and the zone lives in settings
(`src/shell/qml/PanelContent.qml:24-102`;
`src/shell_customization/src/applet_placement_validator.cpp:35-63`). Commit
already collapses a whole preview into exactly one durable undo step
(`docs/wiki/shell/layout-profiles.md:158-161`), and the final revision is
reserved so an open preview can always commit or cancel
(`src/shell_customization/src/layout_edit_request.cpp:44-57`). Using the
preview as the gesture bracket therefore gives atomic multi-command gestures,
exact cancel, and a single undo entry per gesture for free.

**Rejected.** Preview as a user-toggled mode: only one preview may be active
(`EditingErrorCode::PreviewAlreadyActive`), so a mode-preview would make every
in-drag command a separate durable undo entry, and Escape mid-drag could not
restore the pre-drag state in one revision.

### D3 — Provisional commands fire on target change, never on pointer motion

**Decision [P].** During a drag the editor calls `evaluate()` on every hover
target change to paint accept/reject, and calls `execute()` only when the
resolved drop target identity `(targetPanelId, beforeAnchorId, zone, row)`
actually changes.

**Reason.** Preview undo history is a `QVector<LayoutProfile>` appended per
executed command with no bound
(`src/shell_customization/src/layout_editing_coordinator.cpp:159-169`;
`src/shell_customization/src/layout_editing_repository_p.h:34-36`). Executing
per motion event would append a full profile copy per frame. Keying on target
identity bounds history by the number of distinct targets crossed.

**Rejected.** Time-based debounce — it makes the command stream depend on
machine speed and destroys deterministic replay in tests.

### D4 — `src/profiles` owns user profiles as files; Settings1 owns only the selection

**Decision [P].** A new `Profiles::UserProfileStore` writes derived user
profiles atomically to `$XDG_DATA_HOME/qindaqt/profiles/<id>.json`.
`ProfileCatalog` gains layered loading (system directory, then user directory,
user wins on ID collision) and the shell gains a `--user-profile-dir` /
`QINDAQT_USER_PROFILE_DIR` override matching the existing catalog-path pattern
(`src/shell/common/catalogpaths.cpp:10-26`). Settings1 keeps only
`panels.layoutProfile` (`data/settings/schema-v2.json:143-148`).
**`panels.configuration` stays reserved and unused; it must not become a second
layout store.**

**Reason.** The schema, strict loader, migration path, and structured error
contract already live in `src/profiles`
(`docs/wiki/reference/profile-schema-v1.md:3`). Storing the document under
Settings1 would create a second writer and second validator for the same
format, and would push a ~256 KiB structured document through a transaction
model built for scalar preferences.

**Rejected.** `panels.configuration` as the layout store — it fits inside
Settings1 value bounds (`docs/wiki/reference/settings1-v1.md:69-81`) but
duplicates ownership across module boundaries
(`docs/wiki/architecture/module-boundaries.md:18,32`).

### D5 — The editor is an ordinary top-level; the desktop never enters an edit mode

**Decision [E, already accepted; restated as binding].** `qindaqt-settings`
Customize is an ordinary Wayland top-level linking only public values. It never
links LayerShellQt, never publishes a panel surface, and never receives an
in-process `LayoutEditingRepository`, lease, pointer, or private header across
the process boundary. Live panels never grow edit handles, never become modal,
and there is no global "unlock widgets" toggle.

**Authority.** Manager decision
`ops/team/messages/native-application-design/1787856823-manager-shell-customization-boundary-answer.md`
§Q1.1–Q1.3.

### D6 — Keyboard equivalence lives in the editor window, not on live panels

**Decision [P].** Every customization operation is reachable from the editor
window's keyboard model. No slice attempts keyboard editing on live panel
surfaces.

**Reason.** Production panel windows are created with
`Qt.WindowDoesNotAcceptFocus` (`src/shell/qml/RuntimePanel.qml:15`), so they
cannot take keyboard focus at all. The product constraint that every
pointer-only customization has a keyboard equivalent
(`docs/wiki/index.md:109-110`) is satisfiable today only through the editor.
Introducing a focusable shell interaction surface is a separate decision with
its own ADR and is out of scope for C0–C2.

### D7 — Pointer and keyboard share one pure intent translator, and parity is compiled

**Decision [P].** A pure, Qt-Gui-free static library
`src/apps/settings_center/editor/intent/` maps a
`CustomizationIntent` (`InsertAtTarget`, `MoveAppletToTarget`,
`MovePanelToTarget`, `ReorderPanel`, `ConfigurePanel`, `RemoveApplet`,
`DuplicateApplet`, `SetHideMode`, `SetLayer`, …) plus a resolved
`DropTarget` to an ordered `QVector<EditingCommand>`. Both the pointer path and
the keyboard path call it. A focused test asserts that for every intent the two
input paths produce byte-identical command sequences.

**Reason.** "Every pointer gesture has a keyboard equivalent" is otherwise a
review promise. This makes it a unit-testable invariant with no window, no
compositor and no screenshot.

### D8 — The named-output failure mode must be fixed by a selector resolution pass, not by changing the solver

**Decision [P].** A new pure, always-built module `src/shell_targeting` resolves
each panel's output selector against the live logical output inventory
*before* `PanelLayoutSolver` runs, producing a profile whose every `output`
value names a present output, plus a typed report of migrated and dropped
panels. `shell_layout`'s exact-match contract is unchanged.

**Reason.** Today a named output that is absent fails the **whole** layout
(`src/shell_layout/src/panel_layout_solver.cpp:311-317` →
`PanelLayoutErrorCode::MissingOutput`), and `docs/wiki/shell/panel-surfaces.md:88-92`
records that as deliberate. But "drag this dock to monitor 2" is exactly the
gesture that writes a named selector. Without D8, the first multi-monitor
customization a user performs makes their layout fail to solve as soon as that
monitor is unplugged. Relaxing the solver instead would silently relocate
panels and break the documented fail-closed rule.

**Module placement note.** It cannot live in `src/shell_orchestration`:
that directory is only added under `QINDAQT_BUILD_PRODUCTION_SHELL`
(`src/CMakeLists.txt:54-57`), and the editor must link it in the
dependency-light configuration.

### D9 — The `desktop` placement zone is deferred, explicitly and honestly

**Decision [P].** Dragging applets onto the desktop is **P3**, gated behind a
desktop surface that does not exist. No slice before C4 offers a desktop drop
target, and the editor does not show one.

**Reason.** `desktop` is a valid manifest zone
(`docs/wiki/reference/applet-manifest-schema-v1.md:22`) but no shipped manifest
declares it, profile schema v1 has no desktop container, and the shell has no
background surface or wallpaper renderer. Free x/y placement cannot be added to
`shell_layout`, whose whole contract is edge-anchored collision-free stacking;
a desktop container needs its own pure planner and its own ADR.

### D10 — Multi-row flow needs an explicit persisted row index, not automatic wrapping

**Decision [P].** Profile schema v2 adds an optional per-applet `row` integer,
default `0`, valid `0 <= row < panel.rows`, validated by the loader and by
`shell_customization`. `PanelContent.qml` becomes rows × zones.

**Reason.** `rows` currently only multiplies depth
(`src/shell_layout/src/panel_layout_solver.cpp:80-85`;
`src/shell/qml/PanelSurface.qml:19`) and every shipped profile uses `rows: 1`,
so nothing regresses. Automatic wrapping was rejected because wrap points
depend on rendered text metrics, which would make a pure layout module depend
on fonts and would make the required display matrix non-deterministic
(`docs/wiki/development/testing-harness.md:945-952`).

### D11 — Floating docks are a margin field, not a new layer

**Decision [P].** Schema v2 adds panel `margin` (logical px, `0..64`, default
`0`) and `opacity` (`0.2..1.0`, default `1.0`). `below`/`normal`/`above`/
`overlay` keep their exact ADR-0007 meaning. A reserving floating panel
reserves `margin + rows * thickness`.

**Reason.** `PanelSurfaceConfiguration` already carries `margins` and the
exclusive zone already sums with the anchored-edge margin
(`src/shell_surface/include/qindaqt/shell_surface/panel_surface_configuration.h:42-47`;
`docs/wiki/shell/panel-surfaces.md:69-72`). Only the *authored* value is
missing. `reducedTransparency` clamps `opacity` to `1.0` at the token layer,
consistent with the existing accessibility transform contract.

### D12 — Explicit Apply; the editor never auto-saves

**Decision [P].** Edits mutate the in-memory repository only. **Apply** writes
the user profile and, when the profile identity changed, commits
`panels.layoutProfile` through the Settings1 client. **Revert** discards by
constructing a fresh repository from the last applied profile. Closing a dirty
editor prompts.

**Reason.** The repository deliberately adds no persistence
(`docs/wiki/shell/layout-profiles.md:163`). Auto-save would make cancel
untrustworthy across an output-inventory change or an editor crash, and would
contradict the accepted rule that the shell never persists or auto-commits a
preview (manager answer §Q1.2).

---

## 3. P1 decisions

- **D13 [P] — Reveal producer is a one-logical-pixel trigger surface.** For each
  hidden panel the shell maps a non-reserving, `overlay`-layer, 1-logical-pixel
  trigger surface along the panel's anchored edge, mapped only while that panel
  is hidden. Pointer enter acquires a `Reveal` lease from
  `PanelInteractionStore`; leave releases it after
  `panels.autoHideDelayMs` (`data/settings/schema-v2.json:150-155`).
  *Rejected:* extending Compositor1 to publish global pointer coordinates —
  a new compositor authority and a new privacy surface for no extra user value.
- **D14 [P] — Hold producers.** An open panel context menu, an in-flight
  provisional drag targeting that panel, and an editor "focus this panel"
  action each take a `VisibilityHold` lease. Hold outranks reveal, matching the
  implemented policy (`docs/wiki/shell/panel-visibility.md:54-57`).
- **D15 [P] — The canvas renders the target profile's theme, not the app theme.**
  The editor derives a second `DesignTokens` value with
  `DesignTokenDeriver::derive()` and injects `DesignTokens::toVariantMap()`
  into canvas QML as a plain property. Canvas chrome is plain `QtQuick` bound to
  that map; `QindaQt.Controls` is used only outside the canvas.
  *Reason:* `TokenFacade` is one generation per `QQmlEngine`
  (`src/design_tokens/include/qindaqt/design_tokens/token_facade.h:21-23`) and
  Controls may read nothing but that singleton (`docs/wiki/shell/controls.md`).
  `ThemeCard`'s supplied-preview contract is the precedent.
- **D16 [P] — Editor session is bound to one output generation.** The editor
  obtains the inventory from the shell (C2) or from its own `QScreen` list
  (C0), records a generation value, and cancels + rebuilds the session on any
  change, matching the repository's documented rule
  (`src/shell_customization/include/qindaqt/shell_customization/layout_editing_repository.h:69-71`).
- **D17 [P] — `always` hide mode is disabled in the editor until C1 lands**, with
  a `DegradedNotice` naming the reveal slice. Offering a mode that makes a panel
  permanently unreachable would be a dishonest control.

## 4. P2 decisions

- **D18 [P] — Cross-process provisional layout uses canonical schema-v1 JSON
  bytes, re-validated by the shell.** The wire payload is
  `LayoutProfile::toJson()` output; the shell re-loads it through
  `ProfileLoader::fromJson()`
  (`src/profiles/include/qindaqt/profiles/profile_loader.h:21`). The shell
  trusts the strict loader, not the sender, and needs no new validator.
  Payload cap 262,144 bytes, matching the Settings1 per-value aggregate bound.
- **D19 [P] — The shell owns the D-Bus name.** `org.qindaqt.ShellLayout1` is
  owned by `qindaqt-shell`; the editor is a client. The shell is the resident,
  restart-once authority (ADR-0019) and must fail closed; the editor is
  transient and may crash. This also matches every existing QindaQt transport
  (resident owner + exact-owner client with owner/epoch/revision lineage).
- **D20 [P] — Same-user authorization is sufficient; no descriptor token.**
  The shell checks `GetConnectionUnixUser` equals its own UID and grants one
  provisional lease at a time. *Reason:* provisional layout exposes no private
  material and mutates nothing durable, unlike notification presentation
  (ADR-0011/ADR-0020) which required token provisioning. Settings1 already uses
  plain same-user session authority (`docs/wiki/reference/settings1-v1.md:7`).
- **D21 [P] — Drop onto the *real* panel surface is C2b, not C2.** C2 already
  makes real docks change live while the user drags on the canvas. Literal
  cross-surface Wayland drag-and-drop onto a layer surface additionally
  requires a drag MIME contract, live panels accepting drag input, and a
  hit-test authority in the shell — three new contracts for an incremental gain.
  It is designed below but sequenced after C2.

## 5. P3 decisions

- **D22 [P]** — Desktop container and desktop applets (see D9), after a
  background surface exists.
- **D23 [P]** — Panel fragment and whole-profile drag sources in the palette
  (drag a "GNOME top bar" fragment onto an edge). These are `AddPanel` with a
  prefabricated `PanelSpec` and need no new engine capability; they are
  deferred only for scope.
- **D24 [P]** — Import/export of user profiles as files, and profile
  duplication from the built-ins.
- **D25 [P]** — `panel-fill` zone reachability. `zoneForSettings()` maps only
  `start`/`center`/`end`
  (`src/shell_customization/src/applet_placement_validator.cpp:53-62`), so the
  `panel-fill` zone declared by `global-menu`, `notification-center` and
  `task-list` manifests is unreachable from schema v1. Fixing it is a v2 zone
  vocabulary change; until then the editor must not offer a "fill" zone.

---

## 6. Process and module boundaries

```
 qindaqt-settings (ordinary Wayland top-level)         qindaqt-shell (layer surfaces)
 ┌──────────────────────────────────────────┐          ┌──────────────────────────────┐
 │ editor/ui   (QML: palette, canvas,       │          │ shell_orchestration          │
 │              outline, inspector)         │          │  + PanelInteractionStore     │
 │ editor/intent  [P] pure intent→command   │          │  + provisional adapter [P]   │
 │ editor/model   [P] session, dirty state, │  D-Bus   │ shell_surface (LayerShellQt) │
 │                apply/revert, persistence │◄────────►│ shell_visibility(_client)    │
 │ shell_customization  [E] engine          │ ShellLay │ applet_runtime               │
 │ shell_layout [E] · shell_targeting [P]   │ out1 [P] │ shell_targeting [P]          │
 │ profiles [E] · applets [E]               │          │ profiles · shell_layout [E]  │
 │ design_tokens · controls [E]             │          └──────────────────────────────┘
 │ services/settings_client [E]             │                     ▲
 └──────────────────────────────────────────┘                     │
                    │  panels.layoutProfile                       │ committed profile
                    ▼                                             │
          org.qindaqt.Settings1 [E]        $XDG_DATA_HOME/qindaqt/profiles/*.json [P]
```

New rows for `docs/wiki/architecture/module-boundaries.md` **[P]**:

| Module | Owns | Allowed inward dependencies |
| --- | --- | --- |
| `src/shell_targeting` | Pure output-selector resolution, migration/drop reporting, and selector policy values | Public `profiles` and `shell_layout` values plus Qt Core; never surfaces, D-Bus, QML, or the solver's internals |
| `src/apps/settings_center/editor/intent` | Pure customization intent vocabulary, drop-target resolution, and intent→`EditingCommand` translation | Public `profiles`, `applets`, `shell_layout`, `shell_customization` values plus Qt Core; never Qt Gui/Quick, D-Bus, or persistence |
| `src/apps/settings_center/editor/model` | Editor session lifetime, dirty/applied state, apply/revert, user-profile persistence, Settings1 selection | The intent library, public `profiles` store, and the public Settings1 client; never QML types or shell headers |
| `src/services/shell_layout_protocol` | `ShellLayout1` versioned values, bounds, and codecs | Qt Core and serialization-only Qt DBus |
| `src/services/shell_layout_client` | Exact-owner asynchronous provisional lease/push/clear with timeout, backoff, and stale-reply rejection | The protocol module plus Qt Core/DBus; never shell or surface objects |

**Direction rules [P]:** the editor never links `shell_surface`,
`shell_orchestration`, `shell_visibility_client`, or LayerShellQt. The shell
never links the editor's model or intent libraries. Both link
`shell_targeting`, `shell_layout`, `shell_customization`, `profiles`,
`applets`. No module gains a second reason to change.

---

## 7. Transaction schema

### 7.1 In-process (unchanged, reused as-is) **[E]**

`EditingCommand` = `AddPanel | RemovePanel | MovePanel | ConfigurePanel |
InsertApplet | MoveApplet | RemoveApplet | DuplicateApplet |
UpdateAppletSettings | Undo | Redo | BeginPreview | CommitPreview |
CancelPreview` — `src/shell_customization/include/qindaqt/shell_customization/editing_commands.h:15-134`.
Every command carries `expectedRevision`; a mismatch is rejected before any
copy or validation (`:137-139`). Results carry
`EditingErrorCode` ∈ {`RepositoryNotReady`, `ReentrantExecution`,
`StaleRevision`, `RevisionExhausted`, `InvalidCommand`, `DuplicatePanelId`,
`DuplicateAppletId`, `UnknownPanelId`, `UnknownAppletId`, `UnknownAnchorId`,
`InvalidManifest`, `ManifestUnavailable`, `UnsupportedAppletPlacement`,
`InvalidProfile`, `InvalidLayout`, `PreviewAlreadyActive`, `PreviewNotActive`,
`NothingToUndo`, `NothingToRedo`, `NoChange`}
(`.../editing_result.h:11-33`).

**The editor adds no command kind in C0–C1.** Every gesture is expressed as an
ordered sequence of the fourteen existing kinds.

Canonical gesture → command sequences **[P]** (produced by the D7 translator):

| Gesture | Sequence |
| --- | --- |
| Insert applet from palette | `BeginPreview`, `InsertApplet{panelId, instanceId, pluginId, initialSettings{zone,row}, beforeAppletId}`, `CommitPreview` |
| Reorder applet inside one zone | `BeginPreview`, `MoveApplet{same panel, beforeAppletId}`, `CommitPreview` |
| Move applet across zones | `BeginPreview`, `MoveApplet{…}`, `UpdateAppletSettings{settings with new zone}`, `CommitPreview` |
| Move applet to another panel | `BeginPreview`, `MoveApplet{sourcePanelId,targetPanelId,beforeAppletId}`, `UpdateAppletSettings{zone}` when the zone differs, `CommitPreview` |
| Duplicate applet | `BeginPreview`, `DuplicateApplet{newAppletId}`, `CommitPreview` |
| Remove applet | `BeginPreview`, `RemoveApplet`, `CommitPreview` |
| Create panel on an edge | `BeginPreview`, `AddPanel{PanelSpec, beforePanelId}`, `CommitPreview` |
| Move panel to another edge / monitor | `BeginPreview`, `MovePanel{outputId, edge, alignment, beforePanelId}`, `CommitPreview` |
| Restack panels on one edge | `BeginPreview`, `MovePanel{same edge/output, new beforePanelId}`, `CommitPreview` |
| Change thickness/rows/length/layer/hide | `BeginPreview`, `ConfigurePanel{layer,hideMode,rows,thickness,length}`, `CommitPreview` |
| Delete panel | `BeginPreview`, `RemovePanel`, `CommitPreview` |

`ConfigurePanelCommand` replaces all five fields at once
(`src/shell_customization/src/panel_edit_mutation.cpp:146-152`), so the editor
must always send the complete current tuple with one field changed. This is a
correctness trap worth an `AGENT-GUARD` at the call site.

### 7.2 Cross-process `org.qindaqt.ShellLayout1` version 1 **[P, needs ADR]**

- Name/interface `org.qindaqt.ShellLayout1`, object `/org/qindaqt/ShellLayout1`,
  owned by `qindaqt-shell`. Authority: same-user session, one provisional lease.
- `GetLayoutState() -> (status, wireVersion, epoch, shellRevision,
  outputGeneration, outputs[], committedProfileId, provisionalActive, message)`
  — `outputs[]` is the shell's exact `LogicalOutput` inventory so the editor's
  repository is built on the shell's generation, never a second one.
- `AcquireProvisional(clientDescription) -> (status, epoch, leaseId, message)` —
  `Busy` when another lease is live.
- `SetProvisional(leaseId, epoch, editorRevision, profileJson) ->
  (status, shellRevision, message)` — the shell re-validates through
  `ProfileLoader::fromJson`, resolves selectors (D8), solves, plans, and
  reconciles. Any failure returns typed status and **leaves the previously
  published surface set intact**.
- `ClearProvisional(leaseId, epoch)`, `ReleaseProvisional(leaseId, epoch)`.
- Signal `ProvisionalStateChanged(epoch, shellRevision, outputGeneration, active)`.
- Statuses: `Applied`, `Rejected`, `Busy`, `NoLease`, `EpochMismatch`,
  `StaleRevision`, `OutputGenerationChanged`, `Malformed`, `TooLarge`,
  `Unavailable`.
- Bounds: profile JSON ≤ 262,144 bytes; one request in flight per lease;
  the client rejects a missing or non-v1 `wireVersion`.
- Lifetime: lease loss, disconnect, `NameOwnerChanged`, timeout, editor exit or
  crash, malformed payload, output-generation change, or shell restart
  discards provisional state and atomically reconciles the last committed
  profile. The shell never persists or auto-commits a provisional layout.
  (Manager answer §Q1.2, restated as protocol.)

---

## 8. WYSIWYG interaction state machine

One state machine drives pointer, keyboard, and (later) touch. `R` is the
repository revision.

```
        ┌────────┐  press on source            ┌─────────┐
        │  Idle  ├────────────────────────────►│ Arming  │
        └───▲────┘  (or Enter on palette item) └────┬────┘
            │                                       │ movement ≥ threshold
            │ CommitPreview ok                      │ (or Space in outline)
            │                                       ▼
     ┌──────┴───────┐   drop on accepted    ┌──────────────┐
     │  Committing  │◄──────────────────────┤   Dragging   │◄──┐ hover change
     └──────────────┘                       └──┬───────────┘   │ evaluate()
            ▲                                  │               │
            │ CancelPreview                    │ target change │
     ┌──────┴───────┐  Escape / release off    │ execute()     │
     │  Cancelling  │◄─────────────────────────┴───────────────┘
     └──────────────┘  / lease loss / output generation change / preview error
```

| State | Entry action | Engine calls | Exit |
| --- | --- | --- | --- |
| `Idle` | — | none | press/Enter → `Arming` |
| `Arming` | capture source identity and grab origin | none | threshold → `Dragging`; release → `Idle` (treated as click/select) |
| `Dragging` | `BeginPreview{expectedRevision=R}`; on failure abort to `Idle` with a typed toast and **no visual drag** | `evaluate()` per hover-target change; `execute()` per resolved-target change (D3) | drop on accepted target → `Committing`; Escape/off-target release/error → `Cancelling` |
| `Committing` | apply any trailing `UpdateAppletSettings`, then `CommitPreview` | ≤2 commands + commit | → `Idle`, one durable undo step, dirty flag set |
| `Cancelling` | `CancelPreview` | 1 command | → `Idle`, profile exactly as before the gesture |

Invariants **[P]**:

1. `Dragging` is entered only after `BeginPreview` succeeded. There is never a
   visual drag without an open preview.
2. `CancelPreview` can never fail from revision exhaustion, because the final
   revision is reserved (`src/shell_customization/src/layout_edit_request.cpp:44-57`).
3. Undo/Redo controls are disabled in `Arming`/`Dragging`/`Committing`/
   `Cancelling` and enabled only in `Idle`.
4. Acceptance highlights are discarded on **any** revision change; they reserve
   nothing (`.../layout_editing_coordinator.h:26-29`).
5. Keyboard move mode uses the identical state machine with `Space` for
   arm/drop and `Escape` for cancel, and emits identical commands (D7).
6. An output-generation change in any non-`Idle` state forces `Cancelling`,
   then destroys and rebuilds the session (D16).

---

## 9. Collision, rollback, and persistence rules

**Concurrency**

| Scope | Mechanism | Failure |
| --- | --- | --- |
| One editor thread | Repository is not thread-safe by contract (`layout_editing_repository.h:36-41`) | Undefined if violated; enforce with a thread-affinity assert |
| One coordinator | `tryAcquireCoordinator()` move-only lease **[E]** | Null result; editor shows read-only banner and retries on next action |
| Re-entrancy | `ReentrantExecution` guard **[E]** | Typed failure; never a partial mutation |
| One provisional lease per session | `AcquireProvisional` **[P]** | `Busy`; second editor stays canvas-only |
| Optimistic revision | `expectedRevision` on every command **[E]** | `StaleRevision`; editor refreshes targets and retries the gesture, never replays blindly |

**Rollback**

- In-process: every candidate passes typed validation, a strict schema-v1
  serialize/load round trip, and a complete all-output solve before publication;
  a failure leaves snapshot, revision, preview, and history untouched
  (`docs/wiki/shell/layout-profiles.md:155-157`;
  `src/shell_customization/src/layout_editing_coordinator.cpp:142-144,171-175`). A
  multi-output edit is therefore never half-applied.
- Cross-process: a rejected `SetProvisional` keeps the prior published surface
  set, mirroring the existing hotplug rule
  (`docs/wiki/shell/panel-surfaces.md:25-27`).
- Gesture: `CancelPreview` restores the exact pre-preview profile in one
  revision **[E]**.
- Apply: the user-profile write is atomic (`QSaveFile`, ADR-0022 precedent). If
  the write fails, the profile selection is **not** committed and the editor
  stays dirty with a typed error. If the write succeeds but the Settings1
  commit is uncertain, the editor resyncs from Settings1 rather than replaying
  (`docs/wiki/reference/settings1-v1.md:41-46`).

**Persistence**

- Durable customization = a derived user profile file (D4). Built-in data
  stays immutable (`docs/wiki/shell/layout-profiles.md:28-29`).
- Selection = `panels.layoutProfile` through the public Settings1 client.
- The shell reads the layered catalog at startup; ADR-0019 (restart the
  production shell once) governs how a newly applied profile reaches a running
  session in C0 — until C2, applying a profile takes effect at the next shell
  start, and the editor must say exactly that rather than implying live effect.
- Nothing in this lane writes `panels.configuration`.

---

## 10. Responsive, multi-output, and DPI matrix

Editor window rows (offscreen unless noted) **[P]**:

| Row | Window / scale | Expected | Gate |
| --- | --- | --- | --- |
| E1 | 1920×1080 @100% | Palette + canvas + inspector, three panes | `qindaqt.customize-offscreen` |
| E2 | 1920×1200 @100% | As E1 | same |
| E3 | 2560×1440 @100% | As E1; canvas content column capped | same |
| E4 | 1920×1080 @125% | Logical layout identical to E1 | visual row (ADR-0021 one process per row) |
| E5 | 1920×1080 @150% | As E4 | visual row |
| E6 | 1280×800 | Inspector collapses to a drawer; canvas keeps both axes | offscreen |
| E7 | 900×600 | Palette collapses to a rail; outline remains reachable | offscreen |
| E8 | 720×480 (minimum) | Single-column, canvas scrollable, every action reachable by keyboard | offscreen |

Canvas topology rows — the canvas must render each of these correctly and the
editor must produce a solvable layout for each **[P]**:

| Row | Topology | Must prove |
| --- | --- | --- |
| T1 | Single 1920×1080 @100% | Baseline placement, all four edges, all four alignments |
| T2 | Dual 1080p horizontal | Panel move between outputs; per-output arrangements differ |
| T3 | Dual 1080p vertical | Same, vertical adjacency |
| T4 | Mixed 1080p @100% + 1440p @150% | Logical thickness constant across scales; canvas labels each output's scale |
| T5 | Portrait WUXGA beside landscape 1440p | Vertical panels on the portrait output; corner ownership |
| T6 | Negative-coordinate origin | No coordinate wrap; matches the solver's existing negative-coordinate coverage (`docs/wiki/shell/layout-profiles.md:100-103`) |
| T7 | Overlapping (mirrored) outputs | Wildcard expansion produces one surface per output, not per pixel region |
| T8 | Named-output panel whose output disappears | `shell_targeting` migrates or drops per policy; the rest of the layout still solves (D8) |
| T9 | Output added mid-session | Editor session cancels and rebuilds; no provisional survives (D16) |

Every row must also be exercised across all ten built-in profiles for the
whole-shell gate, matching the existing required matrix
(`docs/wiki/development/testing-harness.md:922-943`). Focused pairwise coverage
per change; the complete matrix is a release gate.

**DPI rule [E, restated].** Geometry is logical; scale is metadata and is never
multiplied into thickness (`docs/wiki/shell/layout-profiles.md:80-85`). The
editor therefore offers no per-scale thickness override; the canvas instead
renders each output at true relative logical size and labels its scale, so a
32-px panel visibly reads smaller on a large logical output.

---

## 11. Accessibility and direct-manipulation parity

**Parity contract [P].** For every operation in §7.1's gesture table there is a
keyboard path emitting the identical command sequence (D7), and a focused test
asserts it. Default bindings:

| Operation | Binding |
| --- | --- |
| Insert selected palette item at the insertion point | `Enter` |
| Enter/exit keyboard move mode on the selected item | `Space` |
| Move within the current zone | `Ctrl+Left/Right` (horizontal panel), `Ctrl+Up/Down` (vertical) |
| Move to the previous/next zone | `Alt+Left/Right` |
| Move to the previous/next row | `Alt+Up/Down` |
| Move to the previous/next panel | `Ctrl+Shift+Left/Right` |
| Move the selected panel to the previous/next edge | `Ctrl+Alt+Left/Right/Up/Down` |
| Move the selected panel to the previous/next output | `Ctrl+Alt+PageUp/PageDown` |
| Remove | `Delete` |
| Duplicate | `Ctrl+D` |
| Undo / Redo | `Ctrl+Z` / `Ctrl+Shift+Z` and `Ctrl+Y` |
| Cancel the in-flight move | `Escape` |
| Apply / Revert | `Ctrl+Return` / `Ctrl+Shift+Return` |

These are editor-window shortcuts only. They are **not** registered with
KGlobalAccel; ADR-0009 reserves that path for shell-wide actions and neither
profile data nor applet QML may claim bindings.

**Assistive-technology model [P].**

- The canvas is a *rendering*. It is `Accessible.ignored`.
- The **outline view is the accessible representation** of the layout: a tree of
  outputs → panels → rows → zones → applet instances, each with
  `Accessible.role`, `name`, `description`, and position-in-set. Every
  operation is performed from the outline, so no capability is pointer-only.
- Drag feedback is announced politely on target change and assertively on
  rejection, in the form "Move Clock to Top bar, end zone, position 3 of 4 —
  accepted" / "… — rejected: applet does not support vertical placement". The
  reason text is the `EditingError.message` already produced by the engine.
- Announcements coalesce through the next event turn and publish exactly one
  latest tuple, reusing the `StateCard` pattern (`docs/wiki/shell/controls.md`).
- Focus is rendered from QST `focus.ring` and is never removed.
- `reducedMotion` removes the animated drag ghost and snaps; `reducedTransparency`
  makes drop highlights and canvas panel fills opaque; `highContrast` and
  `accessibility.textScale` flow from the existing token transforms.
- Offscreen tests assert the accessible tree shape and the announcement
  sequence, following the notification precedent
  (`docs/wiki/development/testing-harness.md:385-405`). A live AT-bridge run
  remains a separate display-matrix gate and is not claimed here.

---

## 12. Phased slices

Each slice is one user-visible outcome, one worktree, one candidate commit, and
one different-worker review.

### C0 — Customize: a real drag-and-keyboard layout editor with persistence (**recommended first slice**)

**User outcome.** From Settings → Customize, a user sees an accurate WYSIWYG
canvas of their current layout across every monitor; drags applets from a
palette onto panels; drags applets and panels between zones, edges and
monitors; creates, deletes, restacks, resizes and reconfigures panels; does all
of it from the keyboard; undoes and redoes; and applies the result as their own
saved layout.

**Why first.** It needs no new cross-process protocol, no profile schema
change, and no production-shell build option — `shell_customization`,
`shell_layout`, `profiles`, `applets`, `design_tokens` and `controls` are all
unconditional in `src/CMakeLists.txt:3-7,21-22,47`, so C0 builds and tests in the
dependency-light CI lane (`docs/wiki/development/testing-harness.md:77-81`).

**Steps, in order.**
1. **D1 manifest coverage.** Add the 21 missing manifests to `data/applets/`.
   This is a precondition, not a separate slice: it has no independent user
   outcome, and without it more than half of the first gestures fail.
2. `src/shell_targeting` (D8) with selector policy values and resolution.
3. `editor/intent` pure translator (D7).
4. `editor/model`: session, dirty/applied state, `UserProfileStore` (D4),
   apply/revert (D12), Settings1 selection.
5. `editor/ui`: palette, canvas (D15 token injection), outline (§11),
   inspector, state machine (§8).

**Paths owned.** `data/applets/**` (additive), `src/shell_targeting/**`,
`src/apps/settings_center/editor/**`, `src/profiles/**` (user store only),
`tests/shell_targeting/**`, `tests/apps/settings_center/**`, additive lines in
`src/CMakeLists.txt`, `tests/CMakeLists.txt`, `mkdocs.yml`.

**Invariants.** No new `EditingCommand` kind. No placement policy in QML. No
`shell_surface`/LayerShellQt link. No writes to `panels.configuration`. Editor
QML contains no hex literals or theme identities (source-policy gate).

**Acceptance commands [P] — for the implementer to run, not results.**
```sh
cmake --preset dev && cmake --build --preset dev
ctest --test-dir build/dev -R '^qindaqt\.(profile-|applet-|shell-customization-|shell-targeting-)' --output-on-failure
ctest --test-dir build/dev -R '^qindaqt\.customize-' --output-on-failure
ctest --test-dir build/dev -R '^qindaqt\.customize-source-policy$' --output-on-failure
cmake --build --preset dev --target all_qmllint
mkdocs build --strict
```
**Required evidence.** `qindaqt.applet-catalog-profile-coverage` proves the
uncovered plugin-ID set is empty across all ten profiles.
`qindaqt.customize-intent-parity` proves pointer and keyboard emit identical
command sequences for every gesture in §7.1. `qindaqt.customize-state-machine`
proves §8's six invariants including cancel-restores-exactly and
one-undo-step-per-gesture. `qindaqt.customize-persistence` proves atomic user
profile write, layered catalog precedence, failed-write leaves selection
unchanged, and uncertain Settings1 commit resyncs. `qindaqt.customize-offscreen`
covers rows E1–E3, E6–E8 plus the accessible tree and announcement sequence.
Visual rows E4–E5 follow ADR-0021 (one process per row). Report exact counts
and exit status; report any matrix row not run.

**Risks.** (a) Canvas fidelity drift from real surfaces — mitigated because the
canvas renders the same `PanelLayoutResult` the shell consumes, and a test
asserts canvas rectangles equal solver rectangles. (b) Unbounded preview
history — mitigated by D3; add an explicit history bound test. (c) `data/applets`
is a shared registry — additive-only, with board notice to the applet lane.

### C1 — Reveal, hold, and hide animation

**User outcome.** A hidden dock reappears when the pointer reaches its edge and
stays while a menu or drag is active; `always` becomes an honest choice.

**Content.** D13 trigger surfaces, D14 hold producers,
`panels.autoHideDelayMs` consumption, hide/show animation gated on QST
`motion.*`, and removal of the D17 `DegradedNotice`.

**Paths.** `src/shell_orchestration/**`, `src/shell_surface/**`,
`src/shell/runtime/**`, `tests/shell_orchestration/**`, `tests/shell/**`.

**Acceptance [P].**
```sh
ctest --test-dir build/dev -R '^qindaqt\.(shell-visibility|shell-orchestration|shell-surface)-' --output-on-failure
ctest --test-dir build/dev -R '^shell\.production-surface\.(1080p|wuxga|1440p)$' --output-on-failure
ctest --test-dir build/dev -R '^shell\.reveal\.(1080p|wuxga|1440p)$' --output-on-failure
```
The new nested rows must observe an `always` panel unmapped, a pointer entering
the trigger region, the panel mapping, the hold surviving an open menu, and the
panel unmapping after the configured delay — as committed protocol state, not a
screenshot (`docs/wiki/development/testing-harness.md:273-301` is the pattern).

**Risk.** A trigger surface must never reserve work area and must never appear
in the reservation-carrier calculation; `PanelSurfaceRuntimePlanner` requires an
exact identity bijection with the base plan
(`src/shell_surface/include/qindaqt/shell_surface/panel_surface_runtime_planner.h:55-60`),
so trigger surfaces must be a separate surface class, not extra panel entries.

### C2 — Live provisional binding (`org.qindaqt.ShellLayout1`)

**User outcome.** Real docks and panels change while the user drags in
Customize; cancel restores them instantly; closing or crashing the editor
restores the committed layout.

**Content.** D18–D20, `src/services/shell_layout_protocol`,
`src/services/shell_layout_client`, a shell-side provisional adapter in
`src/shell_orchestration`, and **one ADR** covering authorization, lineage,
bounds, timeout, crash, and the never-persist rule (the manager already
required this ADR). Next free ADR number is **0026**
(`docs/wiki/adr/index.md`; 0018 is an unused hole and must not be reused).

**Acceptance [P].** Private-`dbus-daemon` client tests for exact-owner binding,
epoch fencing, stale replies, `Busy`, timeout/backoff; a nested row proving a
provisional push changes committed layer-surface state and that killing the
editor restores the committed set within the timeout.

### C2b — Drop onto the real panel surface (D21)

Only after C2. Adds a drag MIME contract, live-panel drag acceptance, and a
shell-side hit test. Needs its own ADR because it makes live panels interpret
foreign drag data.

### C3 — Profile schema v2

**Content.** Per-applet `row` (D10), panel `margin` and `opacity` (D11), output
selector object and policy (D8 persistence half), derived-profile metadata
(`derivedFrom`, author, timestamps), and the `panel-fill` zone vocabulary
(D25). Loader migration v1→v2 with tests, updated
`docs/wiki/reference/profile-schema-v1.md` → v2 page, and editor controls for
each new field.

**Why v2 is unavoidable.** Schema v1 validates unknown fields as syntax but
does not preserve them through a model round trip
(`docs/wiki/reference/profile-schema-v1.md:20-21`), and every
`shell_customization` candidate is forced through a strict round trip
(`docs/wiki/shell/layout-profiles.md:155-157`). Any new persisted field must
therefore be a real schema field.

### C4 — Desktop container and desktop applets (D9/D22)

Background-layer desktop surface, a separate pure desktop placement planner,
`desktop` manifests, and editor drop targets. Own ADR.

### C5 — Whole-shell customization qualification (`QQ-004.09`)

The complete §10 matrix across all ten profiles at 1080p/WUXGA/1440p and
100/125/150%, plus keyboard and accessibility passes, in the isolated nested
session.

---

## 13. Dependency order and coordination

```
D1 manifests ─┐
              ├─► C0 editor ──► C1 reveal ──► C2 live binding ──► C2b real-surface drop
D8 targeting ─┘         │                              │
                        └────────► C3 schema v2 ───────┴──► C4 desktop ──► C5 qualification
```

- C0 blocks nothing else and is blocked by nothing.
- C1 is independent of C0 and may run in parallel by a different worker; it
  only removes C0's D17 degraded notice.
- C3 must not start before C0 lands, or the editor UI and the schema change
  will collide in the same files.
- Shared registries (`src/CMakeLists.txt`, `tests/CMakeLists.txt`,
  `mkdocs.yml`, `data/applets/`, source-shape config) are additive-only edits
  with board notice, per `AGENTS.md`.
- Cross-lane: the app-shell lane (`src/appshell`) owns navigation chrome;
  Customize registers as a route rather than inventing its own window
  scaffolding. The applet lane owns `data/applets` semantics; D1 is additive
  data plus a test and needs its notice. The display lane owns Display1 stable
  identities (ADR-0017); C3's output selector should consume that public value
  contract rather than duplicating identity logic.

---

## 14. Prohibited shortcuts

An implementer must not:

1. Re-implement placement, zone, orientation, or collision policy in QML or in
   the editor. `evaluate()` is the only source of drop-target acceptance.
2. Pass a `LayoutEditingRepository`, coordinator lease, raw pointer, or private
   header across the process boundary (manager answer §Q1.1).
3. Link LayerShellQt, `shell_surface`, or `shell_orchestration` from the editor,
   or map a layer surface from `qindaqt-settings`.
4. Auto-commit or persist a preview, or adopt one on timeout (§Q1.2).
5. Relax `AppletPlacementValidator` to make unmanifested drags succeed instead
   of shipping manifests (D1).
6. Change `PanelLayoutSolver`'s missing-named-output failure into a silent
   relocation (D8).
7. Store a layout document in `panels.configuration`, or introduce a second
   profile JSON writer outside `src/profiles` (D4).
8. Add a persisted profile field without a schema version bump, migration, and
   loader tests — unknown fields do not survive the mandatory round trip (C3).
9. Register editor shortcuts with KGlobalAccel (ADR-0009).
10. Put a global "edit mode" toggle on the desktop, add handles to live panels,
    or make live panels modal during customization (D5).
11. Offer a `hideMode` or zone the runtime cannot honor — `always` before C1
    (D17), `panel-fill` before C3 (D25), `desktop` before C4 (D9).
12. Claim canvas rendering, an offscreen test, or a screenshot as evidence for
    live layer-surface behavior; protocol state is the evidence
    (`docs/wiki/development/testing-harness.md:292-301`).
13. Bypass `expectedRevision` by re-reading the current revision inside the
    command builder — that defeats optimistic concurrency by construction.

---

## 15. Items needing an owner's consent before implementation

| Item | Owner to consult |
| --- | --- |
| New `data/applets/*.json` entries (D1) | applet-runtime/manifest owner |
| New module rows in `docs/wiki/architecture/module-boundaries.md` | manager |
| `src/shell_targeting` as a new always-built module (D8) | shell-layout owner |
| ADR-0026 for `ShellLayout1` (C2) | manager; ADR number confirmed free at base |
| Profile schema v2 (C3) | profiles owner |
| Trigger-surface class in `shell_surface` (C1) | shell-surface owner |
| Customize as an app-shell route | app-shell / settings-center lane |

---

## 16. Coverage honesty

Everything in §1 and every citation in this document is reproducible by reading
files at `9db68c4023257b49421101fa1b13c73bbc2cfa85`. Sections 2–15 are
proposals: `src/shell_targeting`, the editor libraries, `ShellLayout1`, the
user-profile store, the reveal producers, schema v2, the desktop container, and
every named test do not exist. No slice, matrix row, budget, or acceptance
command in this document has been executed, and no statement here is evidence
that any code behaves as documented.

I did not compile, run tests, launch any UI or session, start a compositor, or
touch the host desktop, input devices, session bus, or user configuration. My
only writes were this thread and my own live-board record.

— Liora Vale, 2026-08-28T13:11:01Z
