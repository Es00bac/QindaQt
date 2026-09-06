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
| Output Selector | Output cards with connector name, model, resolution, refresh rate, and primary badge | Projected from authoritative Display1 snapshot |
| Output State | Enable/Disable switch | Validates that at least one output remains enabled in the proposed topology |
| Resolution & Mode | Advertised mode dropdown list | Modes advertised by compositor for the selected connector |
| Scale | Segmented presets (100% – 300% in fractional increments) | Validated against protocol scale constraints (1.0× to 3.0×) |
| Transform | Orientation dropdown (0°, 90°, 180°, 270°) | Normal, 90°, 180°, 270° clockwise rotation |
| Primary Output | "Make Primary" toggle / button | Designates primary output for default desktop surfaces and taskbars |
| Topology / Position | Position coordinate controls (X, Y) | Validates contiguous, non-overlapping canvas bounds |

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
  -R '^qindaqt\.(display-settings-model|display-settings-model-adversarial|display-page)$'
```

- `qindaqt.display-settings-model` verifies snapshot projection, connected-but-disabled output enable drafts and reset, scale/transform/position mutations, and full confirm/revert transaction cycles.
- `qindaqt.display-settings-model-adversarial` verifies rejection of invalid topologies (all outputs disabled, overlapping outputs), stale lineage recovery, service crash handling, and stage rejection.
- `qindaqt.display-page` verifies offscreen QML page rendering, including selecting and enabling a connected disabled display, output-card keyboard selection, control interaction, preview banner countdown actions, and degraded notice display.
