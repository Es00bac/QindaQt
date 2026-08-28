# Power applet presentation model

`src/shell/power_applet` owns the presentation-only projection that a future
panel Power applet renders: battery summary and per-supply rows, charge
states, bounded time-remaining truth, critical/low/full severity, brightness
control rows, and the brightness control request lifecycle. It consumes only
public PB-0 values — [`power_protocol`](../reference/power1-v1.md) and
[`brightness_model`](../architecture/brightness-model.md) — plus Qt Core. The module is pure:
no QObject, no QML, no transport, no clocks, no files, and no power or
brightness policy of its own. The accepted authority split lives in
[Power and brightness architecture](../architecture/power-service.md) and the
applet resolution rules in [Applet runtime](applet-runtime.md).

Current maturity: **presentation source candidate**. The P1 slice is
source/static only: it has focused hostile tests and a boundary gate that run
after the registry seams below are wired, and it claims no compiler, CTest,
GUI, session, bus, or hardware evidence. It does not advance the Power
platform milestone (QQ-005.03) by itself.

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

## Registry seams (pending, additive)

The candidate compiles only after the manager wires these one-line additive
seams; none is edited by this slice:

1. `src/CMakeLists.txt`: `add_subdirectory(shell/power_applet)` guarded like
   the shell subdirectories.
2. `tests/CMakeLists.txt`: `add_subdirectory(shell/power_applet)`.
3. `docs/wiki/architecture/module-boundaries.md`: one source-ownership row.
4. `docs/wiki/development/testing-harness.md`: test-matrix rows for the three
   registered tests.
5. Later applet integration: manifest catalog entry, capability policy, the
   compiled built-in registry and QML dispatcher entry, and a shell-private
   facade exposing this model, per [Applet runtime](applet-runtime.md).

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
| `qindaqt.power-applet-boundary` | Static policy gate rejecting transport, QML, QObject, platform, and hardware tokens outside the declared include roots. |

The boundary gate also runs without configure:

```sh
cmake -DSOURCE_ROOT=<repository> -P tests/shell/power_applet/check_boundary.cmake
```

## Non-claims

This slice proves no compiled applet, panel surface, QML component, Power1
service or client, brightness mutation, session action, hardware access, or
runtime qualification. It owns no policy: aggregation, estimates, thresholds,
and capability truth remain Power1 and composition authority.
