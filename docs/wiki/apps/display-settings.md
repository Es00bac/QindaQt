# QindaQt Settings — Display route

`qindaqt-settings --page display` is the first-party Display settings surface.
It is a modular route within the `qindaqt-settings` Qt Quick application,
composed beside Notifications and Appearance. The domain module
`src/apps/settings/display` owns validated display draft values, the reactive
preview/layout projection, and the Display1/DisplayCoordinator-backed route model.
The Settings Center executable owns only the additive route registration and lifetime.

## What the route offers

The Display settings route provides comprehensive monitor and layout management:

| Section | Controls | Authority & Behavior |
| --- | --- | --- |
| Output Selector | Numbered output cards with connector name, model, resolution, refresh rate, and primary badge | Projected from authoritative Display1 snapshot; the number is the card's inventory position and marks the same display in the arrangement |
| Output State | Enable/Disable switch | Validates that at least one output remains enabled in the proposed topology |
| Arrangement | Scaled diagram of every enabled display at its logical size, pointer drag with edge snapping, arrow-key nudges, quick placement (Left of / Right of / Above / Below a reference display), a live position readout, and a "Not in use" strip for connected-but-disabled outputs | Drafts through `setOutputPosition`; the diagram derives logical size from mode, scale, and rotation with the topology module's rounding so it stays truthful while a draft is rejected |
| Exact position | Position coordinate fields (X, Y) as an optional precision control | Accepts negative coordinates; validation normalizes the applied topology so its top-left display sits at 0,0 |
| Resolution & Mode | Advertised mode dropdown list | Modes advertised by compositor for the selected connector |
| Scale | Segmented presets (100% – 300% in fractional increments) with per-display guidance: the current preset's logical size, whether it divides into whole pixels, and the typical preset for the display's resolution | Validated against protocol scale constraints (1.0× to 3.0×); only the chosen preset carries the amber fill |
| Transform | Orientation presets (0°, 90°, 180°, 270°) | Normal, 90°, 180°, 270° clockwise rotation |
| Primary Output | "Make Primary" toggle / button | Designates primary output for default desktop surfaces and taskbars |

Output cards are Tab-focusable radio controls activated by pointer, Return,
Enter, or Space. Coordinate text is an explicit edit session: Return, Enter,
or focus loss commits one valid integer to the output where the edit began.
Invalid input is rejected and resynchronized. Output selection and externally
refreshed draft truth also resynchronize the fields, so stale text cannot be
applied to another output or resurrect a configuration that the service
reverted.

A connected display that is currently disabled remains in this selector. Its
card clearly states that state; selecting it exposes **Enable display**. Enabling
it retains the advertised mode and places the new draft to the right of the
existing non-mirrored desktop so the user can review and apply a valid layout,
or use Revert to restore the disabled snapshot.

## Arranging displays

The Arrangement section is the primary way to position monitors; the exact
coordinate fields remain as a precision control. All of it is presentation over
the public `DisplaySettingsModel` facade (`outputs`, `setSelectedOutputId`,
`setOutputPosition`, `setOutputScale`) and the existing Apply / Keep / Revert
transaction. Nothing reaches the compositor while dragging; a drop, a nudge,
or a quick placement only edits the draft.

- **Logical sizes.** Each enabled display is drawn at pixel size ÷ scale,
  transposed for 90°/270° rotation, using the same round-half-up rule as
  `DisplayTopology::logicalSizeForMode`. The diagram derives that size itself
  because the facade's `logicalWidth`/`logicalHeight` are only refreshed when a
  draft is accepted; a mixed 4K @ 200% beside 1080p @ 100% therefore shows two
  equal 1920 × 1080 tiles.
- **Numbering.** Tiles carry the inventory number shown on the output cards.
  Clicking or pressing Space/Return on a tile selects that display exactly as
  the card does. Connected-but-disabled outputs appear in a "Not in use" strip:
  selectable, never draggable.
- **Snapping.** A dragged display always lands attached to another enabled
  display (sharing an edge, so the pointer can cross) and never overlapping,
  which is the topology's admission rule; a gap only warns. Within a small
  screen-pixel threshold the free axis also aligns with a neighbour's edge.
  Negative coordinates are ordinary results of dropping a display left of or
  above the origin.
- **Keyboard.** Tiles are Tab-focusable. Arrow keys slide the focused display
  along the edge it touches in 10-pixel steps (100 with Shift) without
  alignment snapping; nudging into a neighbour or away from it keeps the
  display attached. Quick placement puts the selected display Left of, Right
  of, Above, or Below a reference display (a selector appears when more than
  one other display is enabled), so every attached position is reachable
  without a pointer.
- **Size changes.** When one enabled display changes logical size in place
  (scale, mode, or rotation), the section re-attaches its neighbours: displays
  touching its right or bottom edge follow the new edge, a row or column
  propagates, and shrinking pulls them back, so repeated scale changes never
  drift. The primary display never moves as a side effect; a non-primary
  display whose only neighbour sits to its right (or below) keeps that shared
  edge instead. Detection runs synchronously on `outputsChanged`; the position
  edits run on the next event-loop turn and are dropped if the geometry changed
  in between. Snapshot resets and reverts (`draftDirty` false) never trigger a
  repair, so applied truth is left alone.
- **Scale guidance.** The Scale row states the current preset's logical size
  and whether it divides into whole pixels, each preset's accessible
  description carries the same facts, and a caption names the typical preset
  for the display's resolution (resolution-only: the facade does not expose
  physical size, so density-aware advice is a Terra-side facade addition).

The page follows the editing sequence directly: choose a display, arrange it,
choose its resolution and scale, then set orientation. The fixed bottom action
bar keeps Apply and Revert available while the form scrolls. Service recovery
is offered once in the status notice rather than duplicated beside these
editing actions; closing belongs to the Settings window chrome and is not
repeated inside the page.

The page is built strictly using QindaQt.Controls primitives and QST-1 semantic
roles, with comprehensive accessibility descriptions and visible focus chains.

## Truthful state and transaction flow

The Display route uses the public asynchronous `QindaQt::Display::Client` and
`QindaQt::Display::Coordinator` to execute safe, reversible display layout changes:

1. **Drafting** — User interactions update the local draft candidate. Changes are
   validated locally against geometric and protocol invariants before submission.
2. **Staging & Preview** — Applying changes invokes the coordinator transaction:
   - The coordinator requests `Stage` on the display service with the candidate topology.
   - Upon acceptance, the coordinator requests `Preview`, temporarily applying the layout
     with a server-managed timeout countdown (typically 15–30 seconds).
   - An alert banner displays the remaining seconds with explicit **Keep Changes**
     (Confirm) and **Revert** (Cancel) actions.
3. **Confirmation / Revert** — If the user confirms, the transaction commits permanently.
   If the user cancels or the timer expires without confirmation, the service reverts to
   the previous known-good topology snapshot automatically.
4. **Lineage and Invalidation** — If the server reports a new snapshot or owner/epoch
   replacement during drafting or transaction, the client updates its internal baseline
   and invalidates or rebases pending mutations.
5. **Degraded & Offline States** — If the display service is unreachable or unowned on the
   bus, the route presents an accessible degraded notice with retry capabilities.

## Composition seam

Inside `qindaqt-settings`:

1. The route descriptor `display` is registered with title "Display", category "Hardware",
   icon "preferences-desktop-display", and component kind `SettingsRouteKind::Display`.
2. One `QindaQt::Display::Client` and `QindaQt::Display::Coordinator` instance are
   constructed and managed for the process lifetime.
3. `DisplaySettingsModel` wraps the client/coordinator and exposes a clean, QML-safe
   property interface to `DisplayPage.qml`.
4. `SettingsRouteHost` dynamically instantiates `DisplayPage` when selected via navigation
   or `--page display` command line argument.

## Verification

Focused test selection:

```sh
ctest --test-dir build/dev --output-on-failure --no-tests=error --parallel 1 \
  -R '^qindaqt\.(display-settings-model|display-settings-model-adversarial|display-page|display-arrangement-canvas|display-arrangement-scale)$'
```

- `qindaqt.display-settings-model` verifies snapshot projection, connected-but-disabled output enable drafts and reset, scale/transform/position mutations, and full confirm/revert transaction cycles.
- `qindaqt.display-settings-model-adversarial` verifies rejection of invalid topologies (all outputs disabled, overlapping outputs), stale lineage recovery, service crash handling, and stage rejection.
- `qindaqt.display-page` verifies offscreen QML page rendering, including selecting and enabling a connected disabled display, output-card keyboard selection, control interaction, preview banner countdown actions, and degraded notice display.
- `qindaqt.display-arrangement-canvas` drives the real model over the fake transport with a 4K @ 200% and a 1080p @ 100% display: tiles at logical size with numbers matching the cards, a drag past the primary that snaps to a negative x and aligns the top edge, a drag that slides along the shared edge and one that cannot overlap, arrow-key nudges that keep focus and attachment, quick placement on all four sides, Revert, and the connected-but-disabled tile that selects but never drags. Set `QINDAQT_DISPLAY_ARRANGEMENT_RENDER_DIR` to keep PNG renders of each checkpoint.
- `qindaqt.display-arrangement-scale` verifies the scale guidance text and preset descriptions, neighbour re-attachment after 100% / 150% / 200% with no drift, an untouched left neighbour and a rotation that keeps the arrangement valid, propagation along a row of three, and that Revert and a fresh service snapshot never trigger a repair.
