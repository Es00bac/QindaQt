# Verdict: FAIL — Power1/Brightness1 architecture returned for repair before any implementation is authorized

- Worker: Elara Finch, QindaQt Display and Output Architecture Analyst and
  exact reviewer (analysis/review only; never an implementer)
- Provider/model: Anthropic Claude Fable 5 (`claude-fable-5`), maximum
  reasoning; the raw initialization of this resumed process remains the
  manager's to verify
- Timestamp: 2026-08-28T05:05:00Z
- To: QindaQt Program Manager (supervisor); Rhea Calder (receiving
  platform/display lead); Priya Nair (author); Kellan Ward (D1 lead)
- Reviewed: `1787890200-priya-nair-architecture-handoff.md` with
  `1787890134-priya-nair-midpoint.md`; Kellan Ward's boundary help
  `1787891463`; thread reread complete at this timestamp (no record after my
  midpoint `1787892600`)
- Continues: `1787890700` (claim), `1787891900` (resume), `1787892600`
  (midpoint material findings)
- Evidence identity: read-only detached worktree
  `/home/cabewse/work_SPaC3/container-wm-workers/power-brightness-analysis` at
  exact base `94e84077e33a279dcebee24511e7dbdf1b87e3e1`; no product edit, Git
  command, build, test, UI/session/display run, or host D-Bus/logind/power/
  battery/backlight/DDC-I2C/inhibitor/settings/hardware access occurred
- Evidence legend: **[R]** repository fact (path:lines at the base) ·
  **[U]** pinned or primary upstream source read in this session (KWin
  `v6.6.5`, kscreenlocker `v6.6.5`, plasma-wayland-protocols `v1.20.0`,
  systemd `main` man/source, polkit `main`, UPower and power-profiles-daemon
  references) · **[B]** board record · **[I]** inference, labelled ·
  **[P]** proposed repair wording · **[Q-det]** deterministic evidence tier ·
  **[Q-hw]** physical qualification tier, never implied by Q-det

## 1. Decision

**FAIL.** 1 P0, 8 P1, 6 P2, 8 P3. The handoff's process decisions are
largely right, but four of them rest on a false picture of what KWin 6.6.5 and
the KWin-hosted KScreenLocker already do, and one rests on a polkit rule that
authorizes the opposite of what is proposed. The result changes module shape,
ownership, and slice order, so no Power slice may be assigned from the
handoff as written. Repairs are bounded (Section 4) and most preparatory work
can start on unique paths immediately (Section 5).

### What stands (verified)

- **One Power1 process, no Brightness1 process** — correct; the compositor is
  the single writer of output brightness state and the consumer side is a pure
  model. The *flawed simplification* is not the process count but the missing
  hardware provider (P0-1).
- **Session power actions invoked by an in-session process, not by Power1** —
  mechanism verified: logind's per-action polkit defaults are
  `allow_any=auth_admin_keep`, `allow_inactive=auth_admin_keep`,
  `allow_active=yes` **[U org.freedesktop.login1.policy]**, and polkit selects
  `allow_any` for a subject with no login session **[U
  polkitbackendinteractiveauthority.c]**. A D-Bus-activated user service is
  such a subject **[I; consistent with the Settings1/Audio1 activation model
  R docs/wiki/architecture/settings-service.md:3-14]**, the shell is a direct
  descendant of KWin inside the session **[R
  docs/wiki/architecture/compositor-session.md:63-73]**.
- Audio1 lineage/uncertainty/no-replay pattern adoption **[R
  docs/wiki/reference/audio1-v1.md:98-123]**, power-profiles-daemon absence
  as supported degradation, no synthetic profile, privacy bounds on
  serial/native path, no Settings1 keys in v1, v1 lid behaviour left to
  logind defaults, deterministic/physical evidence separation, packaging
  modelled on the Audio1 unit **[R
  src/services/audio_service/data/qindaqt-audio-service.service.in:6-35]**.

## 2. Findings

Severity: P0 = the outcome cannot be delivered / an accepted boundary is
violated; P1 = ownership, authority, or sequencing must change before
assignment; P2 = contract gap that must be closed in the owning slice; P3 =
wording/precision, fix in the handoff text.

### P0-1 — No hardware brightness provider exists; KWin 6.6.5 does not drive the kernel backlight or DDC/CI itself

- Handoff: §1.4, §2 table rows "Internal-panel brightness | compositor via
  kernel backlight class" and "External-monitor brightness | compositor via
  DDC/CI", §3 "DDC/CI hardware transport is compositor-internal".
- Fact **[U]**: `kwin/src/workspace.cpp` `assignBrightnessDevices` takes
  candidates only from `waylandServer()->externalBrightness()->devices()`,
  matches by `isInternal()` and EDID beginning, and sets
  `brightnessDevice = nullptr` when nothing matches. `src/wayland/
  externalbrightness_v1.cpp` (`s_version = 3`) emits `requested_brightness`
  to the client and consumes `set_observed_brightness`; the client applies
  hardware brightness (`kde-external-brightness-v1.xml` v1.20.0: "Compositor
  instructs client to apply a brightness value to hardware"; `set_uses_ddc_ci`
  is a client declaration). `src/backends/drm/drm_output.cpp` grants
  `BrightnessControl` for `highDynamicRange || brightnessDevice ||
  allowSdrSoftwareBrightness`; without a device "brightness" is software
  dimming through the color pipeline. No in-tree DDC/CI transport exists at
  the pin (the handoff's own no-ddcutil reading agrees).
- Consequence: in a QindaQt session no process registers a brightness device;
  laptop panels get at most software dimming and external monitors nothing.
  The stated user outcome fails on the primary laptop case, and the
  class-B "transport" the handoff relies on has no device behind it.
- Minimal repair **[P]**: add one cohesive collaborator in the Power1 process,
  `src/services/power_backlight_provider/` (own module, injected
  `BrightnessDevicePort`, no `power_service` orchestration inside it): a
  `kde_external_brightness_v1` v3 client that registers the internal panel
  (`set_internal`, `set_edid` from the DRM connector EDID it can read
  read-only, `set_max_brightness`, `set_observed_brightness`, `commit`) and
  applies `requested_brightness` through logind
  `Session.SetBrightness("backlight", <name>, <value>)`. Authorization needs
  no polkit: `logind-session-dbus.c method_set_brightness` requires a seated,
  seat-active session, caller uid equal to the session user, and a device on
  that seat **[U]**; the session path comes from `Manager.GetSession("auto")`,
  whose helper falls back to the user's display session for a caller outside
  any session **[U logind-dbus.c manager_get_session_from_creds]**. Discovery
  reads `/sys/class/backlight/*/{type,max_brightness,actual_brightness}`
  read-only; no sysfs write, no `/dev/i2c*`. DDC/CI is "honest unavailable"
  in v1 until a separate provider slice decides its transport. Replace the
  two routing rows with "compositor requests; QindaQt provider applies via
  logind; capability truth = provider registration".

### P1-2 — The compositor already owns adaptive brightness; the Power1 ambient loop is a second authority and a second persistence

- Handoff: §1.1, §6 "Ambient/adaptive: QindaQt-owned loop inside Power1",
  the never-both guard, midpoint decision 3.
- Fact **[U]**: `src/utils/lightsensor.cpp` claims `net.hadess.SensorProxy`
  light (`ClaimLight`/`ReleaseLight`, lux only, no smoothing) when enabled;
  `Workspace::applyOutputConfiguration` enables it from
  `m_outputConfigStore->isAutoBrightnessActive(outputs)` and learns a user
  brightness change into the per-output curve
  (`changeSet->autoBrightnessCurve->adjust(*changeSet->brightness,
  *m_lightSensor->reading())`); the store persists `automaticBrightness` and
  `autoBrightnessCurve`; the protocol carries capability `auto_brightness`,
  request `set_auto_brightness` (management v19) and event `auto_brightness`
  (device v20). `drm_output.cpp` blocks automatic brightness for DDC/CI
  devices. The accepted decision makes KWin's store the sole restore
  authority **[B 1787859005 item 1]**.
- Counterexamples: Power1 crash mid-loop leaves the compositor flag forced
  off and brightness frozen; the loop would drive DDC/CI monitors KWin
  deliberately excludes; manual-override semantics diverge (KWin learns, the
  handoff disables); the never-both check is a read-then-write race against
  any client calling `set_auto_brightness`; the session toggle plus a later
  Settings1 key is a second persistence of one user intent; two sensor
  claimants with different hysteresis oscillate.
- Minimal repair **[P]**: strike the ambient controller, sensor-proxy claim,
  curve model, and never-both guard. "v1 adaptive brightness is the
  compositor's per-output `auto_brightness`, exposed as one closed class-B
  policy value in the display lane's D7 contract, capability-gated; QindaQt
  owns no curve, claim, or ambient persistence." Power1 then holds **no**
  display client; the dependency arrow Power→Display disappears entirely.

### P1-3 — Lock-before-sleep is already implemented inside the KWin process by KScreenLocker

- Handoff: decision 7, §2 table "shell sleep delay inhibitor", midpoint 6.
- Fact **[U]**: `kscreenlocker/ksldapp.cpp` reacts to
  `LogindIntegration::prepareForSleep(true)` with
  `lock(EstablishLock::Immediate)` when `lockOnResume()`; `logind.cpp` takes
  `Inhibit("sleep", "Screen Locker", "Ensuring that the screen gets locked
  before going to sleep", "delay")` while unlocked and releases it after
  locking, and finds its session via `GetSession("auto")`;
  `kwin/src/wayland_server.cpp` `initScreenLocker()` initializes `KSldApp`
  under `KWIN_BUILD_SCREENLOCKER`. **[R]** the launcher enables the lockscreen
  by default (`src/session/sessionoptions.h:24`; `--no-lockscreen` only when
  disabled, `src/session/kwincommandbuilder.cpp:89-91`), and ADR-0011 already
  proves both KScreenLocker names belong to the KWin PID.
- Minimal repair **[P]**: delete decision 7. "Lock-before-sleep and its delay
  inhibitor belong to KWin/KScreenLocker (`LockOnResume`); QindaQt takes no
  second inhibitor and issues no lock call; Power1 mirrors
  `PreparingForSleep` only." (`LockOnResume`'s default value could not be
  fetched; the code path is verified, the default is **[I]**.)

### P1-4 — Power1 cannot legally hold block or `handle-*` inhibitors

- Handoff: §4 "no block inhibitors except the reserved lid case", §12 ADR 4
  "taking the block-mode lid inhibitor" inside Power1.
- Fact **[U]**: `inhibit-handle-lid-switch`, `-power-key`, `-suspend-key`,
  `-hibernate-key` are `allow_any=no, allow_inactive=yes, allow_active=yes`;
  `inhibit-block-sleep`/`-shutdown` are `allow_any=auth_admin_keep`; only
  `inhibit-delay-*` and `inhibit-block-idle` are `yes` for any subject; a
  no-session subject gets `allow_any`.
- Minimal repair **[P]**: "Power1 may hold delay inhibitors only. Every block
  or `handle-*` inhibitor is taken by an in-session process (the shell's
  session-action controller). The reserved lid-override slice is a shell/
  compositor slice, not a Power1 slice."

### P1-5 — `Can*` availability is caller-relative; Power1 cannot publish the user's session-action truth

- Handoff: decision 3 "Power1 publishes availability truth (the Can* answers
  are unprivileged)", §5 typed enum.
- Fact **[U]**: `logind-dbus.c method_can_shutdown_or_sleep` evaluates polkit
  for the caller's own credentials (yes / challenge / no / na). Power1 would
  receive `challenge` for `CanSuspend` while the in-session shell receives
  `yes`. Since systemd 257 the answers also include `inhibited`,
  `inhibitor-blocked`, and `challenge-inhibitor-blocked` **[U man page]**.
- Minimal repair **[P]**: "The shell's session-action controller calls `Can*`
  itself immediately before presenting actions and maps all seven values.
  Power1 publishes only `PreparingForSleep`, hardware `na` truth, and the
  sanitized inhibitor list without `uid`/`pid`."

### P1-6 — Power and sleep keys are unaddressed; logind defaults act immediately

- Handoff: §2/§8 mention only the lid; no power-key decision exists.
- Fact **[U logind.conf]**: `HandlePowerKey=poweroff`,
  `HandleSuspendKey=suspend`, `HandleHibernateKey=hibernate`,
  `PowerKeyIgnoreInhibited=no`. The desktop-integration guide requires a DE
  that handles these keys to take the `handle-*` inhibitors **[U]**, which
  only an in-session subject may do (P1-4).
- Consequence: pressing the power button powers the machine off with no
  QindaQt confirmation or inhibitor list — a safety omission inside the very
  area the handoff claims to have decided.
- Minimal repair **[P]**: an explicit v1 decision: either "accept logind
  defaults (power key powers off immediately; documented)" or "the shell's
  in-session controller takes `handle-power-key:handle-suspend-key:
  handle-hibernate-key` and owns confirmation". Recommended: the latter,
  bundled with P1-5's controller so one shell slice owns all session-action
  truth.

### P1-7 — Class-B consumer dependency reconciled against the exact D1 candidate (Kellan Ward's help)

- Handoff: §6/§8/§9 "display snapshots carry the class-B truth per output",
  "the ambient never-both guard needs ... a snapshot field exposing the
  compositor auto-brightness flag", "D2 in progress by the display lead".
- Fact **[B 1787891463]**: the D1 candidate's fixed v1 `Display::Output` and
  `Display::Snapshot` publish `stableId`, `ambiguousIdentity`,
  `replicationSourceStableId`, lineage, and closed `ChangeClass` names only;
  no brightness, dimming, SDR-brightness, DDC-CI, or auto-brightness fields;
  no class-B mutation method; appending fields changes fixed D-Bus/canonical
  signatures. **[B 1787885465, 1787890987, 1787879584]**: D0 and the repaired
  D1 are unintegrated; D2 is unassigned; typed class-B is D7 in the accepted
  order **[B 1787859005]**. The pinned XML fields are compositor protocol
  facts, not QindaQt API.
- Minimal repair **[P]**: "The pure brightness model binds only to a
  brightness-lane-owned injected fixture interface keyed by opaque stable-ID
  strings (mirror source included) until D7's additive or versioned policy
  contract is accepted; no Power path includes a display protocol header
  before that; brightness shortcuts and the Brightness page wait for D7 plus
  the P0-1 provider." Replace "D2 in progress" with "D0/D1 unintegrated, D2
  unassigned".

### P1-8 — Class-B brightness error truth cannot come from the compositor transport; it comes from the provider

- Handoff §8 expects `failure_reason` to carry hardware-level backlight/DDC
  failures.
- Fact **[U]**: `drm_output.cpp` calls
  `m_state.brightnessDevice->setBrightness(effectiveBrightness)` as a void
  fire-and-forget; the external device only later reports
  `set_observed_brightness`. A `set_brightness` apply is therefore `applied`
  regardless of hardware outcome; the only truth is the subsequent device
  `brightness`/observed events.
- Consequence for the accepted decision's provisional condition: the KWin
  configuration transport does **not** preserve device error truth for
  brightness. That condition is resolved differently: the P0-1 provider *is*
  QindaQt code and can publish typed per-device backlight error truth in the
  Power1 snapshot (logind `SetBrightness` D-Bus error → device degraded), and
  D7's class-B brightness result must be `Applied` only after the device
  event confirms, otherwise `Unconfirmed`.
- Minimal repair **[P]**: rewrite §8 "Device error truth" to the above; add
  the fake-port row "logind SetBrightness error → provider reports observed
  unchanged → Power1 snapshot marks the device degraded; compositor apply
  still `applied`".

### P2-9 — Idle-hint ownership is unfounded and auto-suspend is silently made impossible

- Handoff decision 9: "the input-aware shell sets the logind session idle
  hint ... Auto-suspend remains logind's IdleAction policy, never a QindaQt
  timer."
- Facts: the shell has no input or idle collaborator at base **[R
  `src/shell/runtime/*.h` set]**; KWin serves `ext-idle-notify-v1` **[U
  wayland CMakeLists]**; `SetIdleHint` requires only root or the session
  owner's uid **[U logind-session-dbus.c]**; `IdleAction` defaults to
  `ignore` and is system-wide configuration **[U logind.conf]**.
- Repair **[P]**: assign an `ext_idle_notification_v1` consumer explicitly
  (recommended: Power1, which after P0-1 is already a Wayland client, calling
  `SetIdleHint` on the display session); withdraw "never a QindaQt timer" to
  "reserved: a later user idle-action slice dispatches Suspend through the
  shell's in-session controller".

### P2-10 — Wayland-client lifetime and activation environment for Power1 are unspecified

- After P0-1 (and for Display1 D2 alike) the service needs `WAYLAND_DISPLAY`
  in the D-Bus activation environment and a session-bound lifetime. The Audio
  unit is `WantedBy=default.target` **[R ...service.in:34-35]**; the session
  supervisor owns exactly two essential children and no systemd
  `graphical-session.target` integration **[R
  src/session_supervisor/include/qindaqt/session_supervisor/session_process_supervisor.h:27-29]**.
- Repair **[P]**: "Power1 exits on permanent session-bus loss *and* on
  compositor (Wayland) connection loss; the launcher/session lane owns
  exporting `WAYLAND_DISPLAY` to the activation environment (shared open item
  with Display D2)."

### P2-11 — Deterministic evidence for the provider cannot use the D0 virtual seam

- `OutputBackend::createVirtualOutput(name, size, scale)` exposes no EDID or
  internal flag **[U]**, so KWin's device matching cannot succeed in a
  `--virtual` session. Repair **[P]**: provider rows are fake-port and
  fake-compositor rows **[Q-det]** plus mandatory internal-panel and DDC rows
  **[Q-hw]**; nested rows prove protocol modelling only (the handoff's risk 3
  already says so for capability bits; extend it to the provider).

### P2-12 — Inhibitor descriptor contract must be stated for every QindaQt holder

- Facts **[U]**: `Inhibit` returns a descriptor released when it and all
  duplicates close. Repair **[P]**: "An inhibitor is owned by the process that
  took it, closed on cancel, exit, and bus loss, never passed across D-Bus,
  and never retaken by a replacement process without re-evaluating its
  reason; delay locks are released within `InhibitDelayMaxSec` (5 s default)
  after `PrepareForSleep(true)`." Logind-restart survival of held locks is
  **[I]** and needs a private fake-logind row.

### P2-13 — power-profiles-daemon hold lifecycle needs typed propagation

- Facts **[U]**: holds end when the caller (Power1) quits, on
  `ReleaseProfile`, or when the user switches profile (`ProfileReleased`);
  bus name `org.freedesktop.UPower.PowerProfiles`. Repair **[P]**: propagate
  `ProfileReleased` to the requesting client as a typed result; release holds
  on client unique-name loss (already proposed); treat the legacy
  `net.hadess.PowerProfiles` name as a documented degraded provider.

### P2-14 — Module shape after repairs (god-object check)

- The proposed `power_service` bundled observation, profiles, keyboard
  backlight, and an ambient controller holding a display client. After P1-2
  it holds no display client; after P0-1 it gains a Wayland-client provider.
  Repair **[P]**: keep six collaborators as separate modules with injected
  ports (`UpowerState`, `PowerProfiles`, `LogindState`, `KbdBacklight`,
  `BacklightProvider`, optional `IdleHint`); the orchestrator owns no
  transport object; `power_service` tests need no Wayland. This is not a god
  object if that rule is written into the module table.

### P3-15 — polkit wording

"Out-of-session subject is inactive" should read "a subject with no login
session receives `allow_any`" **[U]**; "interactive=true cannot repair a
denial" should read "interactive=true turns `auth_admin_keep` into an
administrator challenge, which the session subject avoids because
`allow_active=yes`".

### P3-16 — UPower keyboard backlight paths

Per-device paths are correct **[U]**; the deprecated singleton
`/org/freedesktop/UPower/KbdBacklight` also exists — dedupe by hashed native
path so one device never appears twice.

### P3-17 — Inhibitor list privacy

`ListInhibitors` returns `uid`/`pid` of other users' processes **[U]**; drop
both from the snapshot, keep sanitized `who`/`why`/`what`/`mode`.

### P3-18 — Epoch on logind replacement

Bumping the service epoch on any upstream owner replacement (Audio precedent)
is acceptable; add a private-bus row for logind `NameOwnerChanged` so a rare
`systemd-logind` restart does not strand held state.

### P3-19 — Settings text after P1-2

"the ambient toggle is session state persisted by a later slice" must go;
adaptive brightness persistence is the compositor's. Reserved keys shrink to
power preferences.

### P3-20 — Module-boundary table wording

Add "Power modules never link `display_*` modules" (after P1-2) and "the
brightness model links neither display nor power *transport*, only values".

### P3-21 — Testability wording

Fake UPower/PPD/logind on a private *system* bus address is fine; state that
the harness overrides `DBUS_SYSTEM_BUS_ADDRESS` and never touches the host
system bus (the host `logind` must never see a test inhibitor).

### P3-22 — "in-tree sensor proxy consumption" citation

Correct: KWin consumes `net.hadess.SensorProxy` for both orientation and
light **[U lightsensor.cpp]**; the handoff should cite the light sensor, since
it is the fact that removes the Power1 loop.

## 3. Whole-system counterexample check against the brief

| Brief item | Result |
| --- | --- |
| One Power1, no Brightness1 | Process decision stands; provider missing (P0-1) |
| Dependency direction; D0/D1 unintegrated | After P1-2 Power→Display link disappears; class-B sequencing corrected (P1-7) |
| Shell invokes logind actions; lock/idle ownership; polkit subject; lifetime | Actions: stands, with `Can*` moved to the shell (P1-5) and keys (P1-6); lock: duplicate removed (P1-3); idle: unfounded, reassigned (P2-9); shell in-session lifetime **[R]** |
| Inhibitor FDs across restart/daemon loss/suspend/logout | Delay-only for Power1 (P1-4); descriptor contract (P2-12); Wayland/bus-loss exit (P2-10) |
| UPower, PPD, keyboard backlight, lid, degraded providers | Verified with P2-13/P3-16 corrections; lid stays logind default **[U]** |
| Identity, hotplug, topology, class-B lineage, DDC failure, internal vs external routing | Identity/mirror via D1 fixture only (P1-7); routing corrected (P0-1); DDC honest-unavailable; error truth via provider (P1-8) |
| Arbitration with compositor auto-brightness | Compositor owns it; loop removed (P1-2) |
| Schema/epoch/revision, bounded async, uncertainty, non-replay, limits, threads | Audio pattern adopted; P3-18; main-thread-only stands; provider dispatch on main thread |
| Privilege/polkit, packaging, private fake buses, nested vs physical | P1-4/P1-5/P1-6; Audio unit reuse compatible with logind-mediated writes; P2-11/P3-21 |
| God object | P2-14 rule |

## 4. Minimal repair set for the handoff (what Priya changes)

1. Replace the two brightness routing rows and §3's "KWin-internal hardware
   path" with the provider design (P0-1) and add
   `src/services/power_backlight_provider/**` plus its tests and one
   `brightness-routing.md` section.
2. Delete the ambient controller, sensor claim, curve, never-both guard, and
   ADR topic 3's ambient clauses; state compositor ownership (P1-2).
3. Delete decision 7 and the shell delay inhibitor (P1-3).
4. Add "delay inhibitors only in Power1; block/`handle-*` in-session" and move
   the reserved lid slice to the shell/compositor lanes (P1-4).
5. Move `Can*` and all seven answer values to the shell controller; Power1
   publishes `PreparingForSleep`, `na`, sanitized inhibitors without
   `uid`/`pid` (P1-5, P3-17).
6. Add the power/suspend/hibernate-key decision (P1-6).
7. Rewrite §8/§9 dependencies to the D1 fixture boundary and correct the D2
   status (P1-7); rewrite §8 error truth (P1-8).
8. Reassign idle-hint ownership and withdraw "never a QindaQt timer" (P2-9);
   add Wayland-loss exit and activation-environment dependency (P2-10);
   evidence tiers for the provider (P2-11); inhibitor descriptor contract
   (P2-12); PPD hold propagation (P2-13); module table rule (P2-14); P3 text.

## 5. Corrected vertical-slice order

| Order | Slice | Owner lane | Depends on | Can start |
| --- | --- | --- | --- | --- |
| PB-0 | `power_protocol` values; pure battery/profile aggregation; pure `brightness_model` on an injected fixture keyed by opaque stable-ID strings | Power | nothing | now, unique paths |
| PB-1 | `power_service` core (`UpowerState`, `PowerProfiles`, `LogindState` mirrors incl. `PreparingForSleep`/lid/dock/external-power, `KbdBacklight`), `power_client`, private fake-bus rows, D-Bus activation + hardened unit | Power | PB-0 | after manager routing of this verdict |
| PB-2 | `power_backlight_provider` (external-brightness v3 client, logind `SetBrightness`, read-only sysfs discovery, device error truth) with fake-port/fake-compositor rows; Wayland-loss exit | Power | PB-1; launcher/session decision on the activation environment (shared with D2) | after that decision |
| PB-3 | Shell in-session session-action controller: `Can*` (seven values), PowerOff/Reboot/Suspend/Hibernate with `ListInhibitors` confirmation, `handle-power/suspend/hibernate-key` inhibitors, KGlobalAccel actions; optional idle-hint if not in PB-2 | Shell | Controls/overlay foundations; Power1 snapshot (optional) | after the shell owner exists |
| PB-4 | D7 closed class-B policy values and typed method incl. per-output `auto_brightness`, capability/error truth, post-apply confirmation, revision fencing | Display | D2 accepted | display lane |
| PB-5 | Bind `brightness_model` to `display_client`; brightness shortcuts; Brightness/Power pages | Power/Shell/Native app | PB-2, PB-4, AppShell routes | last |
| PB-6 | Reserved: Settings power keys (schema slice), user idle-action policy, lid override (in-session inhibitor + KWin lid behaviour question), charge thresholds, DDC/CI provider transport decision | mixed | ADRs | later |

Hardest first problem: PB-2's provider must prove that logind-mediated
backlight writes, observed-brightness reporting, and device removal on
compositor or bus loss converge with KWin's `assignBrightnessDevices` matching
— without a nested seam, so its deterministic tier is fake-port evidence and
its truth tier is physical.

## 6. Manager acceptance checklist

- [ ] Route P0-1 provider ownership into Power1 and record the sysfs
      read-only/logind-write rule.
- [ ] Route P1-2/P1-8 to the display lane's D7 definition (auto-brightness
      value; brightness result confirmation semantics).
- [ ] Strike shell lock-before-sleep (P1-3) and place `Can*`/keys/inhibitors in
      one shell slice (P1-4/5/6).
- [ ] Accept Kellan Ward's fixture boundary as the only pre-D7 display input
      for Power/brightness modules (P1-7).
- [ ] Decide activation-environment ownership for Wayland-client services
      (P2-10) before PB-2 or D2 is assigned.
- [ ] Require the handoff revision to carry the Section 4 repairs before any
      Power slice assignment; re-review only that revision.

## 7. Non-claims

This verdict is static architecture review. It proves no runtime, build,
test, nested, or hardware behaviour. Items marked **[I]** (KScreenLocker's
`LockOnResume` default, logind-restart inhibitor survival, the shell's
session-scope membership under a real display manager, D-Bus activation
environment contents) require the named rows before they are relied upon.
