# Bluetooth applet

`src/shell/bluetooth_applet` owns the production panel Bluetooth applet. A pure
target projects bounded Bluetooth1 values and evaluates operation admission; a
separately linked shell-private controller borrows the public
`BluetoothClient` and exposes only owned presentation values to compiled QML.
Neither target imports the Bluetooth service/model, BlueZ, BluezQt, Agent1, or
host-radio APIs.

Current maturity: **qualified production built-in composition (focused
Debug/Release executable and package evidence)**. The B1 slice includes the audited
manifest/registry/policy path, stock-profile placement, production shell
composition, keyboard-accessible compiled QML, static mutation gates, and a
relocated installed-package test. Fresh strict GCC 15.3 Debug and Release roots
each built the production shell and focused targets, passed the seven-row B1
selector, and passed six adjacent public-client, manifest, catalog, resolver,
dispatcher, and shell-catalog rows. Bluetooth B0 still composes its
deterministic empty backend, so a normal activated service truthfully makes the
applet unavailable until the separately reviewed BluezQt runtime adapter
lands. This consumer does not advance platform hardware maturity by itself.

The service authority and pairing exclusion remain those of
[Bluetooth1](../architecture/bluetooth-service.md) and
[ADR-0037](../adr/0037-keep-pairing-and-trust-authority-in-bluez.md). B1 adds no
new cross-cutting decision.

## Exact-owner projection

The controller publishes rows only while all of these facts hold together:

- `bluetooth.read` was granted by the audited manifest/policy evaluation;
- `BluetoothClient` has a non-empty exact unique owner;
- the client has a validated snapshot from that owner in `Ready` state; and
- the snapshot carries the current nonzero epoch and revision.

Owner loss or replacement, a stopped/starting/unavailable client, malformed
truth, or read denial clears every adapter/device row and resets the presented
epoch/revision to zero. Old rows never survive as actionable last-known-good
state. The pure projector validates direct test inputs again, even though the
public client already validates snapshots before publication.

Bluetooth1's bounds remain the applet bounds: at most eight adapters and 256
devices. Rows sort deterministically by opaque handle serial. QML receives
opaque `adapter-<epoch>-<serial>` and `device-<epoch>-<serial>` row IDs, not
Bluetooth addresses, object paths, D-Bus owners, or platform handles. A
reported name is used as the user label; an empty name maps to a deterministic
class or ordinal label without falling back to the address. A non-empty name
that is itself a canonical Bluetooth address is treated exactly like an empty
name, so neither labels nor accessibility text expose it.

Every adapter and device row carries a complete accessible name and state
description. Descriptions spell out power/discovery, paired/connected, and
known battery/signal truth rather than relying on color, ordering, or a glyph.
Unknown optional battery, signal, role, or class values remain unknown and are
never inferred.

## Operation admission and lineage

`bluetooth.control` is independent of `bluetooth.read`: control is effective
only when both are granted. The final controller admission evaluates the
current public snapshot again rather than trusting enabled state sent through
QML.

| Applet action | Admission |
| --- | --- |
| Turn adapter on/off | `SetAdapterPower`, current adapter handle, capability present, requested state differs |
| Start discovery | `DiscoveryLease`, powered current adapter, and no applet-owned lease |
| Stop discovery | `DiscoveryLease` and the exact applet-owned adapter lease |
| Connect | `ConnectPaired`, current paired/disconnected device, and powered current adapter |
| Disconnect | `DisconnectPaired` and current connected device |

The applet exposes no pair, trust, untrust, key, credential, authorization, or
audio-routing action. An unpaired device may be listed with the explicit status
that pairing is unavailable here, but it has no mutation control.

Only one operation is live at a time. Each request pins the unique owner,
operation kind, target handle, initiating epoch, and initiating revision before
one public-client dispatch. The matching request ID and an exact initiating
lineage are required for completion; success additionally requires the same
epoch and an observed revision at least as new as the initiating revision.
Malformed, stale, timed-out, or owner-interrupted results become typed user
uncertainty. Rejection, unsupported, busy, and failed results remain distinct
terminal feedback. No result optimistically changes an inventory row, and no
failure or uncertainty is automatically replayed.

A validated success also does not make the initiating snapshot current. The
controller retains a pending-equivalent control fence until the same exact
owner and epoch publish a validated snapshot whose revision reaches the
result's observed revision. Controls cannot dispatch from the stale initiating
snapshot while that fence is live. Owner, epoch, or snapshot-authority loss ends
the fence as uncertainty and leaves presentation fail-closed; no operation is
replayed.

## Discovery-lease lifetime

The controller tracks at most one lease obtained by this applet. Merely opening
the popup does not begin discovery; the user explicitly starts it. Closing the
popup requests release. If another operation or the acquisition itself is
pending, the release waits behind that one serialized operation and dispatches
once it completes; this deferred teardown is not an operation retry.

An owner replacement or epoch replacement retires the old lease because the
old service authority cannot retain it. A proven adapter power-off or removal
also retires it. A transient snapshot failure with the same owner keeps the
release intent until current truth returns rather than claiming that an
unproven lease disappeared. Shell teardown gives the controller a final
release-dispatch opportunity before stopping its dedicated client; process/bus
caller disappearance is the service-side backstop. A failed or uncertain
release consumes that close/shutdown intent, retains the lease and feedback,
and is never replayed automatically. A later explicit Stop action, a new
open/close cycle, or final shutdown may make one new attempt; current
authoritative truth may instead prove that the lease ended. A malformed or
stale completion that happens to carry a `no-lease` reason cannot retire the
tracked lease; only exact validated success or current authoritative snapshot
truth can do so.

The composition, client, controller, and renderer are GUI-thread confined.
`BluetoothAppletComposition` owns transport → client → controller lifetime;
panel windows borrow only the controller and are destroyed before it. The
client is stopped after the controller's shutdown hook.

## Compiled interaction

`BluetoothApplet.qml` renders a tab-focusable summary button and a non-modal,
Escape-closeable details popup. Adapter power/discovery and paired-device
connect/disconnect are ordinary keyboard-operable buttons with complete
accessible names and descriptions. Failure/uncertainty feedback is exposed as
an accessible alert. Horizontal and vertical panels use the same controller;
the vertical summary uses a compact text label without changing behavior.

The preview injects no live Bluetooth facade and therefore renders a disabled
deterministic fallback. Production composition starts observation only when
read access was granted.

## Production seams

1. `data/applets/bluetooth.json` requests `bluetooth.read` and
   `bluetooth.control`; the stock `qindaqt` profile places one instance in the
   end zone.
2. `BuiltinAppletRegistry::firstParty()` admits
   `qindaqt.applets.bluetooth`; `BuiltinAppletContent.qml` is the independent
   renderer inventory gate.
3. `BluetoothAppletComposition` constructs the public Qt transport/client and
   purpose-specific controller. `RuntimePanelWindowFactory` injects only that
   controller.
4. The `BluetoothAppletRuntime` install component stages the production shell,
   its directly linked Controls library, the Tokens library at Controls'
   baked sibling RUNPATH, compiled QML, manifest, selected profile, policy,
   and theme for relocation under source-path poison. Its package test supplies
   the exact build-selected KF6 GlobalAccel platform artifact inside the
   disposable stage, clears ambient loader paths, resolves the staged
   executable's runtime dependencies, and requires KF6, Controls, and Tokens
   to resolve to their exact relocated artifacts through relative RUNPATHs.
   The shared shell-component closure row separately launches this component,
   the Audio and Power components, and default `QindaQt` in isolation.

## Focused verification

```sh
ctest --test-dir build/dev \
  -R '^qindaqt\.bluetooth-applet-' \
  --output-on-failure --no-tests=error
```

| Test | Scope |
| --- | --- |
| `qindaqt.bluetooth-applet-presentation` | Owner/read failure, bounded deterministic non-address rows, fallbacks, complete accessibility, and action projection |
| `qindaqt.bluetooth-applet-request-state` | All five B0 operations, capability/state admission, exact kind/lineage completion, terminal failure/uncertainty, and no replay |
| `qindaqt.bluetooth-applet-controller` | Public-client projection, grant separation, serialization, exact-owner replacement, typed feedback, and discovery close teardown |
| `qindaqt.bluetooth-applet-offscreen` | Compiled module loading, Space/Escape keyboard paths, accessible buttons, real controller dispatch, and deferred close release |
| `qindaqt.bluetooth-applet-boundary` | Exact five-file/header allowlist plus independent public-client, persistence, filesystem, and adjacent-network poisons |
| `qindaqt.bluetooth-applet-runtime-boundary` | Exact seven-file/header, line-splice/whitespace-normalized controller surface, and literal property/invokable macro-name counts; eleven independent service, single/wrapped/comment-glued invokable, wrapped/paren-gap/line-spliced property, address-accessor, persistence, file, and standard-path poisons |
| `qindaqt.bluetooth-applet-installed-package` | Relocated shell/data, exact staged KF6 and Controls/Tokens loader-path resolution through relative RUNPATH, compiled QML evidence, and installed manifest discovery under source-path poison |

Both static gates can run before configuring a build:

```sh
cmake -DSOURCE_ROOT="$PWD" \
  -DPOISON_ROOT=/tmp/qindaqt-bluetooth-pure-poison \
  -P tests/shell/bluetooth_applet/check_boundary.cmake
cmake -DSOURCE_ROOT="$PWD" \
  -DPOISON_ROOT=/tmp/qindaqt-bluetooth-runtime-poison \
  -P tests/shell/bluetooth_applet/check_runtime_boundary.cmake
```

## Non-claims

This slice proves no BluezQt adapter, host-radio discovery, hotplug,
suspend/resume, physical device connection, Agent1 pairing/prompt UX, trust or
key management, Bluetooth audio correlation/routing, multi-user policy,
AT-SPI bridge behavior, memory/CPU budget, or nested-compositor interaction.
The installed test proves a relocatable composition boundary, not live radio
behavior. Those remain platform, Agent1, audio, accessibility, hardware, and
integrated-session outcomes.
