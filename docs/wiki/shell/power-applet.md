# Power applet

`src/shell/power_applet` owns the pure projection and the shell-private runtime
adapter for the production panel Power applet: battery summary and per-supply rows, charge
states, bounded time-remaining truth, critical/low/full severity, brightness
control rows, profile choices, serialized mutation lifecycles, and an injected
Session controls section. The pure
target consumes only public PB-0 values —
[`power_protocol`](../reference/power1-v1.md) and
[`brightness_model`](../architecture/brightness-model.md) — plus Qt Core. A
separate runtime target borrows the public `PowerClient` and purpose-built
`SessionActionsClient` and exposes bounded values to compiled QML; neither
target reaches into `power_service` or names a platform daemon. The accepted authority split lives in
[Power and brightness architecture](../architecture/power-service.md) and the
applet resolution rules in [Applet runtime](applet-runtime.md).

Current maturity: **production built-in composition (compiled and verified)**.
The P2 slice has a manifest, audited registry entry, production dispatcher and
host injection, keyboard/accessibility interaction, installed-package proof,
and mutation-sensitive boundary gates. The applet presents the current public
Power1/Brightness1 state; unavailable upstream capabilities remain unavailable
in the UI. This consumer does not advance hardware qualification by itself.
Profile and session-action buttons use [QindaQt.Controls](controls.md), preserving
their existing availability, focus, confirmation and action-routing contracts
while following the selected desktop theme.

## Projection contract

`projectPowerApplet(snapshot, powerOwnerAvailable, brightness)` returns one
owned `PowerAppletModel` per call. It is pure, reentrant, and deterministic:
rows are sorted by total identity `(id, epoch)`, so equal inputs produce
equal models regardless of upstream enumeration order.

| Input | Phase | Behavior |
| --- | --- | --- |
| Owner lost | `Unavailable` | Whole model fails closed; a populated snapshot never survives its owner. |
| `wireValid == false` | `Unavailable` | Diagnostic echoes the snapshot diagnostic. |
| Availability `Starting` | `Loading` | Rows stay empty until real truth exists. |
| Availability `Unavailable` | `Unavailable` | Non-empty `reasonCode` becomes the diagnostic. |
| Availability `Ready`/`Degraded` | `Ready`/`Degraded` | Rows project; contradictions degrade without crashing. |

Presentation semantics, pinned by hostile tests:

- **Charge states** map from the closed Power1 vocabulary
  (`Charging`/`PendingCharge` → charging, `Discharging`/`PendingDischarge` →
  discharging, `FullyCharged` → full, `Empty` → empty). Out-of-vocabulary raw
  enumerator values become unknown through a range check before any typed
  switch, so hostile generations cannot trigger undefined behavior.
- **Time remaining** appears only when the upstream known flag holds, the
  value is inside the Power1 estimate bound, and the charge-state direction
  matches the estimate (time-to-empty only while discharging, time-to-full
  only while charging). Estimates are never derived from rate or energy.
- **Severity** maps upstream warning and coarse-level truth one-to-one
  (`Action`/`Critical`/`Low`, `Full` as its own state, `Normal` otherwise).
  Coarse-level semantics apply only when exact percentage is absent. The
  module defines no percentage thresholds; thresholds stay Power1 policy.
- **Hostile numbers** (NaN, infinite, out-of-range percentage or rate)
  degrade to unknown truth and never render.
- **Bounds**: at most eight supply rows are projected; a ninth degrades the
  model instead of rendering unbounded content. A composite without its
  `Supplies` capability bit, or a supply without a valid epoch/ID handle,
  degrades as well.
- **Brightness control rows** come from the composed
  `Brightness::ModelSnapshot` when its owner is available. Without that owner
  the snapshot devices may keep identity-visible rows, but every such row is
  unavailable and non-adjustable. Display rows are never marked adjustable:
  Power1 v1 defines only the keyboard-brightness operation, because display
  brightness is provider-adjusted through KWin in a later slice.
- **Accessibility identity** every row carries a complete accessible name and
  description phrase, including unavailable rows; state meaning is never
  conveyed by color or position alone. Phrases are deterministic English
  source strings; a future QML surface owns localization.

## Brightness request lifecycle

`BrightnessRequest` is a pure value machine with at most one live request:

- `beginKeyboardBrightnessRequest` requires the `KeyboardBacklight`
  capability, a valid device handle, `canSet`, and a nonzero snapshot epoch;
  otherwise it returns a terminal failed request with typed feedback instead
  of a pending one.
- The pending request pins the initiating epoch and revision. A reply whose
  initiating lineage differs is stale for this request and is discarded
  without completing or failing it; the live answer may still arrive.
- Success completes only with an observed generation in the initiating epoch
  at or after the initiating revision. Anything else — `Uncertain` status,
  foreign or earlier observation, malformed reply, out-of-vocabulary status —
  becomes typed uncertain feedback instructing the user path to resnapshot.
- Owner loss or an epoch replacement while pending yields typed
  `OwnerLost` uncertainty. Terminal requests are immutable, and no transition
  ever replays a request automatically, matching the Power1 rule that clients
  resnapshot instead of replaying after timeout or authority loss.

## Runtime composition and interaction

The production shell constructs one shell-private composition that owns a
`QtPowerTransport`, `PowerClient`, and `PowerAppletController` on the GUI
thread. The controller borrows the client; the composition stops and destroys
the client only after panel windows and the controller are gone. `power.read` starts observation, while `power.control`
enables mutation only when read access is also granted. Both grants come from
the same audited manifest/policy evaluation used by applet resolution.

The controller publishes rows only while the client has a validated snapshot
from its exact current owner in `Ready` or `Degraded` state. Owner loss,
replacement, a stopped client, capability denial, or an unavailable snapshot
clears prior battery/profile/brightness truth. Profiles are sorted by ID and
remain inside Power1's four-profile bound. Keyboard brightness uses the pure
composition model and exact 0..10000 normalization; display brightness remains
read-only because Power1 v1 has no display mutation.

Only one profile or keyboard-brightness operation may be pending. Every
gesture resolves its ID in the current snapshot, checks the applicable
capability, and dispatches once. Completion is fenced by request ID, operation
kind, initiating epoch/revision, and observed lineage. Owner replacement ends
the pending state with uncertain feedback; it never replays the request.

Compiled `PowerApplet.qml` renders a compact summary button and non-modal details
popup. The horizontal summary contains a battery-state icon plus its bounded
percentage; vertical panels show only the icon. Its accessible name still
contains the exact percentage and state, and unresolved icon assets use the
typed Power placeholder. Space/Enter activation, tab-focusable profile radio buttons, keyboard
operable brightness sliders, complete accessible names/descriptions, and an
accessible alert for failure/uncertainty feedback are part of the production
contract. The preview injects no live access object and therefore shows a
disabled, deterministic fallback.

The popup's Session section contains Lock, Log out, Suspend, Restart, and Shut
down. Each button follows the client's typed availability and single pending
fence. Lock and Suspend dispatch directly; Log out, Restart, and Shut down
require a focused modal confirmation whose Cancel and OK paths are keyboard
operable. The client calls ScreenSaver on the session bus, authenticated
Session1 for logout, and login1 only after an exact `Can* == "yes"` check.
Owner loss or the five-second mutation deadline produces uncertain no-replay
feedback. The existing audited global-shortcut registrar binds Meta+L to the
same lock request; registration failure leaves the visible path intact.

## Production seams

The module is registered through these additive seams:

1. `data/applets/power.json` requests `power.read` and `power.control`, and the
   stock `qindaqt` profile places the applet in the end zone.
2. `BuiltinAppletRegistry::firstParty()` admits
   `qindaqt.applets.power`; `BuiltinAppletContent.qml` is the separately tested
   renderer gate.
3. `PowerAppletComposition` owns the client/controller lifetime for
   `ShellRuntimeApplication`, and `RuntimePanelWindowFactory` injects only the
   controller into QML.
4. The `PowerAppletRuntime` install component stages the production shell,
   its directly linked Controls library, the Tokens library at Controls'
   baked sibling RUNPATH, compiled QML resources, manifest, selected profile,
   policy, and theme for a relocation/poison test. The shared shell-component
   closure row also proves the default `QindaQt` component and every narrow
   applet component can launch with their own Controls/Tokens payload.

## Focused tests

After the test seam is wired, the focused selector is:

```sh
ctest --test-dir build/dev -R '^qindaqt\.power-applet-' --output-on-failure
```

| Test | Scope |
| --- | --- |
| `qindaqt.power-applet-presentation` | Fail-closed phases, state/severity/time semantics, hostile numbers and raw enums, bounds and capability gates, determinism. |
| `qindaqt.power-applet-control-rows` | Composition-owner fence, availability/reason projection, identity and accessibility phrases, deterministic row order. |
| `qindaqt.power-applet-request-state` | Begin legality, lineage completion rules, stale-reply discard, typed failure feedback, owner-loss uncertainty, terminal immutability. |
| `qindaqt.power-applet-controller` | Public-client projection, read/control denial, bounded profile and brightness dispatch, serialization, result fencing, and owner replacement without replay. |
| `qindaqt.power-applet-offscreen` | Compiled QML loading, keyboard activation, profile/slider/session interaction, destructive confirmation, accessible roles/names/descriptions, and real controller dispatch. |
| `qindaqt.power-applet-boundary` | Static policy gate rejecting transport, QML, QObject, platform, and hardware tokens outside the declared include roots. |
| `qindaqt.power-applet-runtime-boundary` | Runtime source-policy gate rejecting service internals, host daemons, process/file, and hardware access; controller/QML also reject direct D-Bus while the shell root may construct the public Qt transport. Includes a poison negative control. |
| `qindaqt.power-applet-installed-package` | Relocated production shell and data resolve under source-path poison, Controls/Tokens resolve only from the staged relative loader paths, the binary contains the compiled applet module, and `--list` discovers the staged Power manifest. |

The boundary gate also runs without configure:

```sh
cmake -DSOURCE_ROOT=<repository> -P tests/shell/power_applet/check_boundary.cmake
```

## Non-claims

This slice proves no live UPower or power-profiles-daemon adapter, successful
host brightness mutation, display-brightness write, live host power mutation,
idle integration, physical hardware, or nested compositor interaction. It owns no
aggregation, estimate, threshold, or platform policy: those remain Power1 and
brightness-composition authority. Installed proof is relocation and source
policy evidence, not a claim that unavailable PB-1 data has become live.

The details and session-confirmation surfaces use `Popup.Window`, so each extends beyond its originating
panel and receives keyboard focus independently of the panel. Escape and
outside presses dismiss it. Compiled tests traverse the independent content
window for the existing operation and accessibility checks.
