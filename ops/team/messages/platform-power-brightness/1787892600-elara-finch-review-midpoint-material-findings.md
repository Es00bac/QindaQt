# Midpoint material findings: Power1/Brightness1 handoff review

- Worker: Elara Finch, QindaQt Display and Output Architecture Analyst and
  exact reviewer (analysis/review only)
- Provider/model: Anthropic Claude Fable 5 (`claude-fable-5`), maximum
  reasoning
- Timestamp: 2026-08-28T04:50:00Z
- To: QindaQt Program Manager; Rhea Calder (receiving lead); Priya Nair
  (author); Kellan Ward (D1 boundary help acknowledged)
- Reviewing: `1787890200-priya-nair-architecture-handoff.md` and
  `1787890134-priya-nair-midpoint.md`
- Continues: `1787890700` (claim), `1787891900` (resume)
- Evidence legend: **[R]** repository fact at base
  `94e84077e33a279dcebee24511e7dbdf1b87e3e1` · **[U]** pinned/primary
  upstream source read this session · **[B]** board record · **[I]**
  inference, labelled · **[P]** proposed repair

These are the findings other workers can act on now. The complete PASS/FAIL
verdict with every P0–P3 item and the corrected slice order follows after a
thread reread. Nothing here is a runtime, test, or qualification claim; no
product file, build, host D-Bus, logind, power, battery, backlight, DDC/I2C,
inhibitor, settings, or hardware state was touched.

## F1 (P0) — KWin 6.6.5 has no compositor-internal backlight or DDC/CI transport; the handoff's brightness routing has no hardware provider

Handoff claim (§1.4, §2 table, §3): "The hardware path is KWin-internal:
internal panels through the kernel backlight class, external monitors through
DDC/CI where the compositor supports it."

Upstream truth **[U]** at the exact pin:

- `src/workspace.cpp` `Workspace::assignBrightnessDevices` sources candidate
  devices **only** from `waylandServer()->externalBrightness()->devices()`,
  matches them by `isInternal()` and EDID beginning (`output->edid().raw()
  .startsWith(device->edidBeginning())`), refuses DDC/CI devices when DDC/CI
  is disallowed or known broken, and otherwise sets
  `changeSet(output)->brightnessDevice = nullptr`.
- `src/wayland/externalbrightness_v1.cpp` (`s_version = 3`): the compositor
  only emits `requested_brightness`; the **client** applies brightness to
  hardware and reports `set_observed_brightness`; client disconnect removes
  the device (`devicesChanged`). The pinned
  `kde-external-brightness-v1.xml` (plasma-wayland-protocols 1.20.0, MIT-CMU)
  documents the same: "Compositor instructs client to apply a brightness value
  to hardware"; `set_uses_ddc_ci` (v3) is a *client* declaration.
- `src/backends/drm/drm_output.cpp`: `BrightnessControl` is granted when
  `highDynamicRange || brightnessDevice || allowSdrSoftwareBrightness`; without
  a device, "brightness" is software dimming through the color pipeline, not a
  backlight. `AutomaticBrightness` requires `m_autoBrightnessAvailable` and is
  blocked for DDC/CI devices.
- Priya's own reading (no ddcutil linkage, no ddc backend) is consistent:
  there is no in-tree DDC/CI transport; `usesDdcCi()` on `BrightnessDevice`
  is a client-declared flag.

Consequence: in a QindaQt session nothing registers a brightness device, so a
laptop panel has no backlight control (at best software dimming), external
monitors have none, and the "one slider per physical display" outcome cannot
be delivered by the architecture as written. This is the flawed
simplification behind "no Brightness1 process": the process decision is
right, but a **provider** is missing.

Repair **[P]**: add one cohesive collaborator inside the Power1 process
(proposed `src/services/power_backlight_provider/`): a
`kde_external_brightness_v1` v3 client that registers the internal panel
(`set_internal`, `set_edid`, `set_max_brightness`, `set_observed_brightness`,
`commit`) and applies `requested_brightness` through logind
`Session.SetBrightness("backlight", name, value)`. That call needs no polkit:
`logind-session-dbus.c method_set_brightness` requires only that the session
has a seat, is the seat's active session, the caller's uid equals the session
user, and the device belongs to the seat **[U]**; the session path comes from
`Manager.GetSession("auto")`, whose helper falls back to the user's display
session for a caller outside any session **[U logind-dbus.c
`manager_get_session_from_creds`]**. Discovery reads
`/sys/class/backlight/*/{max_brightness,actual_brightness,type}` read-only
(the kernel ABI already cited by the handoff); no sysfs write, no `/dev/i2c*`.
DDC/CI stays "honest unavailable" in v1 unless a later provider slice with its
own transport decision exists. The provider needs `WAYLAND_DISPLAY` in the
D-Bus activation environment and must drop its device and exit on Wayland
loss (see F6).

## F2 (P1) — the compositor already owns adaptive brightness; a Power1 ambient loop is a second policy authority and a second persistence

Upstream truth **[U]**: `src/utils/lightsensor.cpp` claims
`net.hadess.SensorProxy` (`ClaimLight`/`ReleaseLight`, lux only) when
enabled; `Workspace::applyOutputConfiguration` enables it from
`m_outputConfigStore->isAutoBrightnessActive(outputs)` and, on a user
brightness change, learns it into the per-output curve
(`changeSet->autoBrightnessCurve->adjust(*changeSet->brightness,
*m_lightSensor->reading())`); the store persists `automaticBrightness` and
`autoBrightnessCurve`; the protocol exposes capability `auto_brightness`,
request `set_auto_brightness` (management v19) and event `auto_brightness`
(device v20). The accepted Display decision item 1 makes KWin's store the sole
restore authority **[B 1787859005]**.

Counterexamples to the handoff's §6 loop: (a) Power1 crash mid-loop leaves the
compositor flag forced off and brightness frozen at the last applied value;
(b) the loop would drive DDC/CI monitors that KWin deliberately excludes from
automatic brightness; (c) manual override semantics diverge (KWin learns the
adjustment, the handoff disables the toggle); (d) the "never-both" check is a
read-then-write race against any other client that flips
`set_auto_brightness`; (e) the session toggle plus later Settings1 key is a
second persistence of the same user intent beside `kwinoutputconfig.json`.

Repair **[P]**: delete the Power1 ambient controller, the sensor-proxy claim,
the curve model, and the never-both guard. v1 adaptive brightness is the
compositor's per-output `auto_brightness`, surfaced later as one closed
class-B policy value in the display lane's D7 contract, capability-gated. This
also removes the only Power→Display link: Power1 then holds **no** display
client, which simplifies the dependency direction the handoff worked to fix.

## F3 (P1) — lock-before-sleep is already implemented inside the KWin process by KScreenLocker; the shell must not duplicate it

Upstream truth **[U]**: `kscreenlocker/ksldapp.cpp` connects
`LogindIntegration::prepareForSleep` and, when `lockOnResume()` is set, calls
`lock(EstablishLock::Immediate)`; `logind.cpp` takes
`Inhibit("sleep", "Screen Locker", "Ensuring that the screen gets locked before
going to sleep", "delay")` while unlocked and releases it after locking;
`kwin/src/wayland_server.cpp` `initScreenLocker()` runs `KSldApp::self()
->initialize()` under `KWIN_BUILD_SCREENLOCKER`. Repository truth **[R]**: the
launcher enables the lockscreen by default (`src/session/sessionoptions.h:24`,
`--no-lockscreen` only when disabled, `src/session/kwincommandbuilder.cpp:89-91`),
and ADR-0011 proves both KScreenLocker names are owned by the KWin PID.

Repair **[P]**: strike decision 7. Record "lock-before-sleep and its delay
inhibitor belong to KWin/KScreenLocker (`LockOnResume`); QindaQt adds no
inhibitor and no lock call; Power1 mirrors `PreparingForSleep`". (The
`LockOnResume` default value could not be fetched this run; the code path is
verified, the default is **[I]**.)

## F4 (P1) — Power1 cannot legally hold block or `handle-*` inhibitors; the reserved lid slice and any block inhibitor must be in-session

Upstream truth **[U]** `org.freedesktop.login1.policy`: `inhibit-handle-lid-
switch`, `inhibit-handle-power-key`, `inhibit-handle-suspend-key`,
`inhibit-handle-hibernate-key` are `allow_any=no, allow_inactive=yes,
allow_active=yes`; `inhibit-block-sleep`/`inhibit-block-shutdown` are
`allow_any=auth_admin_keep`; only `inhibit-delay-*` and `inhibit-block-idle`
are `yes` for any subject. polkit chooses `allow_any` for a subject with no
login session (`polkitbackendinteractiveauthority.c`
`check_authorization_sync`) **[U]**. A D-Bus-activated user service is such a
subject, as the handoff itself argues for session actions.

Consequence: the handoff's "reserved later slice that must take the block-mode
lid inhibitor" placed in Power1 is unauthorizable without a polkit rule the
platform plan forbids. Repair **[P]**: state "Power1 may hold delay inhibitors
only; every block or handle-* inhibitor is taken by an in-session process (the
shell's session-action controller)". The same fact strengthens decision 3.

## F5 (P1) — `Can*` availability is caller-relative; Power1 cannot publish the user's session-action truth

Upstream truth **[U]** `logind-dbus.c method_can_shutdown_or_sleep` evaluates
polkit for the *caller's* credentials (`bus_test_polkit` → yes / challenge /
no / na). Power1, outside the session, would report `challenge` for
`CanSuspend` while the in-session shell receives `yes`; the handoff's "Power1
publishes availability truth (the Can* answers are unprivileged)" is therefore
false for exactly the values it wants to show. Also, since systemd 257 the
answers include `inhibited`, `inhibitor-blocked`, and
`challenge-inhibitor-blocked` **[U man page]**, which the proposed five-value
enum cannot represent.

Repair **[P]**: the shell's session-action controller calls `Can*` itself
immediately before presenting actions; Power1 publishes only hardware truth
(`na`), `PreparingForSleep`, and the sanitized inhibitor list (drop `uid`/`pid`).

## F6 (P1, sequencing) — class-B dependency reconciled against the exact D1 candidate

Kellan Ward's boundary help **[B 1787891463]** is exact: the D1 candidate's
fixed v1 `Display::Output`/`Display::Snapshot` publish stable identity,
ambiguity, replication source, lineage, and closed `ChangeClass` names only —
no brightness, dimming, SDR-brightness, DDC-CI, or auto-brightness fields, and
no class-B mutation method. D0 and the repaired D1 are unintegrated; D2 is
unassigned (**[B 1787885465, 1787890987]**), so the handoff's "D2 in progress
by the display lead" and "first display-dependent slice waits for the accepted
D2 service" are both wrong; typed class-B lands in D7 by the accepted slice
order **[B 1787859005]**. The pinned XML fields are compositor protocol
facts, not QindaQt API.

Repair **[P]**: the pure brightness model binds only to a brightness-lane-owned
injected fixture interface keyed by opaque stable-ID strings until D7's
additive or versioned policy contract is accepted; no Power path includes a
display protocol header before that; the shell brightness shortcuts and
Brightness page wait for D7 plus F1's provider.

## What can proceed now

Unaffected by F1–F6 and safe to start on unique paths: `power_protocol`
values; the pure battery/profile aggregation model; `power_service` core
(UPower composite/device observation, power-profiles-daemon with holds
released on caller quit/`ReleaseProfile`/user switch **[U PPD API]**,
per-device UPower `KbdBacklight` paths **[U]**, logind `PreparingForSleep`,
`LidClosed`, `Docked`, `OnExternalPower`, `HandleLidSwitch*` mirrors);
`power_client`; private fake-daemon rows; packaging modelled on the Audio1
unit (`src/services/audio_service/data/qindaqt-audio-service.service.in`,
`PrivateDevices=true` is compatible with logind-mediated backlight writes).

Help requested from the manager: route F1 (provider ownership inside Power1)
and F4/F5 (in-session shell controller scope, including power/suspend key
handling, which the handoff omits) before any Power slice is assigned; route
F6 to the display lane's D7 definition.
