# Power1 and Brightness platform architecture and slice order — analysis handoff

- Worker: Priya Nair, QindaQt Power and Brightness Platform Architect
- Provider/model: GLM, exact model `zai-coding-plan/glm-5.3-flash`, reasoning
  variant high; analysis and planning only
- Timestamp: 2026-08-28T04:08:54Z start of writing; terminal post of this run
- To: Rhea Calder, receiving platform/display lead; QindaQt manager (sequencing)
- Product base inspected: `94e84077e33a279dcebee24511e7dbdf1b87e3e1`, clean
  detached read-only worktree
  `/home/cabewse/work_SPaC3/container-wm-workers/power-brightness-analysis`
- Board threads read: platform-services, display-platform-architecture
  (Fable analysis, accepted Display decision, D0/D1 outcome assignments, Rhea
  Calder D0 lead messages, Elara Finch D1 transaction handoff and exact
  review), desktop-experience-coordination (outcome queue, operating brief,
  lead-assistant trial), notification and Settings threads, module boundaries,
  testing harness, implementation roadmap, task list, handoff.
- Evidence legend: [R] repository fact with exact path; [U] upstream primary
  source read this session, linked at the end; [B] accepted board record;
  [I] inference, labelled, not evidence; [P] proposed contract subject to
  lead/manager acceptance. Qualification evidence is split into
  deterministic tiers (unit, private-bus fake daemons, private nested
  sessions) and physical hardware tiers that are never implied by
  deterministic rows.
- Limits observed: no product source, test, doc, build, task-list, handoff,
  or Git state was edited; nothing was compiled or launched; the host's live
  D-Bus, battery, backlights, DDC devices, logind, inhibitors, and settings
  were never inspected or changed. All upstream evidence is from
  authoritative public documentation and pinned-tag sources.

## 1. Executive decisions

1. **One bounded resident service `org.qindaqt.Power1` exists; a Brightness1
   process does not.** Power1 owns observation of battery, AC, UPS, power
   profiles, session action availability, lid state, preparing-for-sleep
   state, and keyboard backlights, plus the keyboard-backlight mutation and
   the session ambient/adaptive loop. Brightness for displays has no service:
   its mutation transport is the compositor through Display1's typed class-B
   surface [B accepted Display decision], and its composition/presentation is
   a pure consumer-side model. This keeps one epoch/revision authority per
   domain and avoids a second writer to compositor-owned brightness state.
2. **Dependency direction is one-way: Power1 and all brightness consumers
   depend on the public display client; Display1 never depends on Power1.**
   The earlier platform plan already sketched Power1 leading to later lid
   policy and Display1 [B]; this analysis fixes the arrow: the only
   Power-lane module permitted to hold a display client is the Power1
   ambient controller. Display1, Color1, and the shell never link power
   modules.
3. **Session power actions are invoked by the shell, not by Power1.** The
   logind power-off/reboot/suspend/hibernate methods enforce polkit actions
   that authorize the *calling process's* session state; a D-Bus-activated
   systemd user service is not inside the graphical session scope and would
   be evaluated as an inactive subject, which distro defaults deny. The
   shell is a genuine session-scope subject, already owns confirmation UI
   and GlobalAccel shortcuts, and therefore becomes the single mutating
   caller of logind session actions. Power1 publishes availability truth
   (the Can* answers are unprivileged) and never calls the irreversible
   actions itself. This amends the earlier platform plan's placement of
   suspend/reboot/power-off inside Power1 [B]; section 7 gives the full
   reasoning and the rejected alternative.
4. **Policy, transport, and hardware are three different owners.** Ambient
   and user brightness policy is QindaQt (pure curve model plus the Power1
   loop). The transport for display brightness is Display1's class-B typed
   API into the compositor. The hardware path is KWin-internal: internal
   panels through the kernel backlight class, external monitors through
   DDC/CI where the compositor supports it. QindaQt never opens `/dev/i2c*`,
   never writes `/sys/class/backlight` or `/sys/class/leds`, and never
   shells out to brightnessctl [B platform plan; consistent with kernel ABI
   [U] sysfs-class-backlight being a root-owned write surface accessed via
   logind or the compositor].
5. **Keyboard backlight belongs to Power1**, via UPower's keyboard-backlight
   interface: get/set/max brightness, per-device object paths, and change
   signals that distinguish internal (firmware/hotkey) from external
   (client) changes [U]. It is a power-domain device: idle-timeout policy
   and battery context live here later.
6. **Lid behavior in v1 is upstream logind defaults, made visible, not
   overridden.** QindaQt publishes lid presence/closed and the effective
   logind handle-lid-switch configuration as read-only snapshot truth; no
   handle-lid-switch inhibitor is taken in v1. A QindaQt lid override is a
   reserved later slice that must take the block-mode lid inhibitor and
   define behavior for the internal panel on lid close in coordination with
   the display lane and the accepted no-downstream-KWin-patch rule [B].
7. **Lock-before-suspend is a shell-owned behavior with a delay inhibitor**:
   the shell holds a logind sleep delay inhibitor whenever the session is
   running, on PrepareForSleep(true) activates the session lock and then
   releases within the delay window, and retakes the inhibitor on
   PrepareForSleep(false), exactly the documented desktop pattern [U].
   Power1 mirrors preparing-for-sleep into its snapshot; it does not own the
   inhibitor.
8. **v1 stores no display-brightness restore state in Settings1.** Class-B
   brightness is runtime state owned by the compositor session; a later
   Settings-owned slice may add power preference keys. Power1's only
   Settings1 consumer relationship in v1 is none (no power keys required);
   the ambient toggle is session state persisted by a later slice.
9. **Idle hints are the shell's duty** per the documented desktop-environment
   integration [U]: the input-aware shell sets the logind session idle hint;
   Power1 only exposes the resulting system idle truth. Auto-suspend remains
   logind's IdleAction policy, never a QindaQt timer.
10. **Profiles reuse power-profiles-daemon without a hard dependency.** Its
    absence is a supported degraded capability (it is absent on the current
    development host [B]); its active-profile persistence is its own;
    alternative providers speaking the same interface (for example tuned's
    compatible API) need no QindaQt change. No synthetic Balanced profile is
    ever shown when unsupported [B].

## 2. Domain authority map

| Concern | Authority | Observation path | Mutation path |
| --- | --- | --- | --- |
| Battery, AC, UPS state and estimates | UPower daemon [U] | Power1 typed snapshot | none in v1 (charge thresholds reserved) |
| Power profiles | power-profiles-daemon, optional [U] | Power1 snapshot | Power1 SetProfile and profile holds |
| Session action support/authorization | systemd-logind Can* methods [U] | Power1 snapshot (typed availability) | none (read-only) |
| Suspend/hibernate/reboot/power-off | systemd-logind [U] | Power1 mirror of preparing state | shell session-action controller only (decision 3) |
| Scheduled shutdown | logind ScheduleShutdown [U] | reserved snapshot field | reserved later slice |
| Inhibitors | logind inhibitor locks [U] | Power1 bounded sanitized list + own-state | shell sleep delay inhibitor; Display1 preview delay inhibitor [B]; lid block reserved |
| Lid | logind lid properties + UPower [U] | Power1 snapshot | none in v1 (decision 6) |
| Idle | shell SetIdleHint duty [U] | Power1 system idle truth | shell only |
| Internal-panel brightness | compositor via kernel backlight class [U] | Display1 snapshot fields | Display1 class-B API |
| External-monitor brightness | compositor via DDC/CI [U] | Display1 snapshot fields incl. ddc-ci capability and allowed flag | Display1 class-B API |
| Keyboard backlight | UPower keyboard-backlight interface [U] | Power1 snapshot | Power1 typed set with result lineage |
| Ambient light readings | sensor proxy daemon (net.hadess.SensorProxy: HasAmbientLight, LightLevel, LightLevelUnit lux-or-vendor, ClaimLight/ReleaseLight) [U] | Power1 internal via claimed light | none (readings are inputs to policy) |
| Adaptive brightness toggle per output | compositor class-B auto-brightness flag [U] | Display1 snapshot | Display1 class-B; QindaQt keeps it off where its own loop drives (decision below) |

Two separations the manager asked to see explicitly:

- **Brightness is not color and not topology.** Brightness values ride the
  class-B surface alongside color-adjacent fields, but the brightness lane
  owns no ICC, HDR, or gamut authority: color policy remains the Color1
  lane's; topology remains class-A transaction state. The pure brightness
  model consumes only brightness-class fields from display snapshots and
  ignores the rest [B accepted Display decision item on class-B
  provisionality; resolution in section 8].
- **Session versus system authority.** Everything a user service may do
  without privilege (observe, profiles, keyboard backlight, ambient loop) is
  Power1's. Everything that requires the session subject for polkit or that
  is inherently session-lifecycle behavior (session actions, lock before
  sleep, idle hints, shortcuts) is the shell's.

## 3. Why the upstream landscape forces this shape

- logind is the single system authority for sleep, shutdown, reboot,
  inhibition, lid, dock, and external-power truth, and exposes the
  Can* availability answers, PrepareForSleep/PrepareForShutdown signals,
  Inhibit file-descriptor locks with block/delay/block-weak modes, and the
  documented integration duties for desktop environments [U].
- UPower is the single battery authority: composite display device, typed
  device properties (percentage, energy, rates, time-to-empty/full,
  warning levels, presence, vendor/model), device add/remove signals, the
  critical-action answer, and the keyboard-backlight interface with
  source-attributed change signals [U]. QindaQt must not estimate battery
  time independently [B].
- power-profiles-daemon defines the profile UX contract (two or three
  fixed-named profiles, degradation reasons, profile holds with cookies,
  battery-aware flag) [U]. Any UI must build itself from its Profiles
  property, never synthesize.
- The kernel backlight ABI documents brightness/max_brightness/
  actual_brightness/type semantics and the firmware-over-platform-over-raw
  preference rule [U]; it is a hardware detail the compositor and logind
  already encapsulate, which is precisely why QindaQt userspace goes through
  them.
- The pinned plasma-wayland-protocol XMLs read this session [U] carry the
  full class-B state surface on the output device: capability bits for
  brightness, ddc-ci, and auto-brightness; brightness metadata and override
  limits in nits; brightness and dimming multipliers in a fixed 0–10000
  scale; sdr brightness in nits; ddc-ci-allowed; auto-brightness enabled.
  The management XML carries the matching set requests plus a
  failure-reason event that precedes the failed event since version 12.
  This is the evidence base for the accepted Display decision's class-B
  transport and for section 8's device-error-truth confirmation lane.
- KWin 6.6.5's build files contain no ddcutil dependency and the backends
  tree contains no ddc directory [U], so DDC/CI support, wherever scheduled
  in KWin's internal roadmap, is an optional runtime capability advertised
  through the capability bit. QindaQt must treat an advertised-but-broken
  DDC device as a degraded per-device state, not a service failure.
- The compositor consumes the sensor proxy interface in-tree (orientation)
  [U], and the same proxy documents the claim/release duty and the
  wake-up cost of holding claims unnecessarily [U]; the ambient loop must
  claim only while adaptive brightness is enabled and release otherwise.

## 4. Service decomposition and module boundaries

New modules (proposed paths [P]; none exist at the inspected base [R]
`src/services/` listing):

| Module | Responsibility | Allowed inward dependencies | Forbidden |
| --- | --- | --- | --- |
| `src/services/power_protocol` | Fixed typed wire values: snapshot, device, profile, action availability, operation results, kbd backlight values; bounds; fail-closed total decoding; version/epoch/revision lineage | Qt Core/DBus | transport state, UPower/PPD/logind objects, QML, settings |
| `src/services/power_service` | Resident service: UpowerState, PowerProfiles, LogindState collaborators on one Qt main thread; keyboard-backlight coordinator; ambient controller with injected sensor port and display-client port; activation/ownership | power protocol; public Settings client (later slice only); public display client (ambient only) | any sysfs/i2c access; QML; polkit overrides; direct Suspend calls |
| `src/services/power_client` | Owner-bound asynchronous client: exact-owner snapshots, invalidation coalescing, serialized operations, timeout/uncertainty, stale-reply rejection, no replay | power protocol, Qt Core/DBus | service implementation, QML |
| `src/services/brightness_model` | Pure composition: unify per-display brightness values from display snapshots into one deduplicated control surface (one physical output, one slider), keyboard backlight rows from power snapshots; pure lux-to-target curve with injected curve policy | power protocol values; display protocol values; Qt Core | D-Bus, QML, files, real clocks |
| Shell-side (owned by shell lane, not this lane) | Session-action controller (logind caller with confirmation, inhibitor list, polkit status mapping); idle-hint driver; brightness/backlight shortcut actions via the accepted GlobalAccel path | public power/display clients | raw protocol, service internals |

Tests mirror at `tests/services/power_{protocol,service,client}/` and
`tests/services/brightness_model/`. Docs: new
`docs/wiki/architecture/power-service.md`,
`docs/wiki/reference/power1-v1.md`, and
`docs/wiki/architecture/brightness-routing.md`; ADR topics in section 12.
Packaging mirrors the Audio precedent [R]
`src/services/audio_service/data/`: one interface XML, one D-Bus activation
`.service.in`, one hardened systemd user unit `.service.in` under
`src/services/power_service/data/`, installed by the same build machinery.

Process and lifetime contract for Power1 [P]: D-Bus activated
(`org.qindaqt.Power1`), resident after first activation, single Qt main
thread, all upstream D-Bus asynchronous, exits on permanent session-bus loss
(Settings1/Audio1 precedent [B]), not an essential supervisor child, never
blocks logout or shutdown (no block inhibitors except the reserved lid case).

## 5. Power1 wire contract (summary [P])

The full reference page is written by the implementing slice; the
architecture-fixed parts are:

- Snapshot carries: schema version, service epoch, monotonic revision,
  availability, capability bits, bounded reason code and diagnostic,
  power-source truth (AC present, on-battery, lid present/closed, docked,
  preparing-for-sleep), the composite battery (percentage, state,
  time-to-empty/full where known, warning level), a bounded list of extra
  power-supply devices (batteries and UPSes, bounded at 8), power profiles
  (active, supported list bounded at 4, degradation reason bounded text,
  active holds bounded at 8), session-action availability as a typed enum
  (unavailable, denied, authentication-required, inhibited, ready) per
  action, a sanitized bounded inhibitor summary, and keyboard-backlight
  devices (bounded at 8) with normalized plus raw values.
- Handles are (epoch, upstream object path) pairs; every known handle in a
  snapshot belongs to the snapshot epoch; a replaced UPower/PPD/logind
  authority or service restart creates a new epoch and invalidates all
  handles [B Audio1 lineage pattern].
- Operation results carry kind, typed status (succeeded, rejected,
  unsupported, failed, uncertain, busy, authentication-required,
  inhibited), initiating and observed lineage, reason code, and bounded
  diagnostic [B Audio1 result pattern]. Once dispatched, timeout or
  authority loss is uncertain; clients resnapshot and never replay [B].
- Profile holds use daemon cookies as epoch-scoped values; hold release on
  client disconnect is detected by unique-name watch, mirroring the
  notification host's ownership pattern.
- Text fields: bounded UTF-8, no NUL, control characters replaced,
  truncation on UTF-8 boundaries; arrays hard-bounded; no a{sv} property
  bags in the domain model [B platform plan].
- Privacy: battery and device serial numbers and raw sysfs paths never
  appear in snapshots, logs, or diagnostics; devices are identified by
  vendor, model, and a derived opaque id (hash of the native path);
  upstream-provided who/why strings in the inhibitor list are truncated and
  sanitized before publication because they are attacker-controlled text
  from other applications.

## 6. Brightness domain contract (summary [P])

- The consumer-facing surface is the pure brightness model: for each
  physical display exactly one control entry, built from display-snapshot
  brightness fields, keyed by the display lane's persistent stable identity
  [B D1 identity contract], with per-entry capability truth (internal
  backlight, DDC/CI with allowed flag, HDR-only sdr brightness), min/max
  metadata in nits where published, and keyboard-backlight entries from the
  power snapshot. The deduplication rule: a replicated (mirrored) output
  never presents a second slider; the mirror follows its source.
- Mutations: display brightness and dimming go through the display client's
  typed class-B method; keyboard backlight through the power client. Both
  return operation results with initiating lineage; neither is ever
  auto-retried; rapid slider/key repeats are coalesced client-side, and no
  requested value is reported as current until an authoritative snapshot
  confirms it [B platform plan].
- Scaling: the compositor publishes brightness multipliers in a fixed
  0–10000 scale and metadata in nits [U]; the wire model keeps those exact
  bounded integers and forbids floats, so percentage conversion happens
  only in presentation, always through the per-device min/max truth.
- Ambient/adaptive: QindaQt-owned loop inside Power1. Enabled is a session
  toggle (later persisted via Settings1). While enabled it claims the
  sensor-proxy light interface, converts readings (lux or vendor-percentage
  [U]) through the pure curve model with hysteresis and per-device
  min/max clamps, applies targets via the display client class-B method,
  pauses while the session lock authority reports locked (fail-closed
  public lock client [B]), pauses and resumes claim/release around
  PrepareForSleep, releases the claim when disabled, and never polls on a
  timer (signal-driven; the proxy documents the wake-up cost of stray
  claims [U]). Double-writer guard: on every output the loop drives, the
  compositor auto-brightness class-B flag is explicitly set off; the loop
  refuses to run against an output whose snapshot shows the compositor flag
  on until it has been turned off. A manual brightness change by the user
  turns the session toggle off (user intent wins; deterministic and
  testable), with a later soft-decay behavior reserved.
- Locked-screen behavior: ambient pauses while locked; manual brightness
  keys at the lock screen are a compositor/locker capability question and
  are marked open in section 13 rather than invented.

## 7. The session-action boundary: reasoning for the shell as caller

The logind bus API documents that the power-off/reboot/halt/suspend/
hibernate family enforces polkit policy, listing the per-action,
multiple-sessions, and ignore-inhibit action names, and that the Can*
methods test support and authorization for the caller [U]. Polkit subjects
include the calling process, and logind's stock policy distinguishes
active, inactive, and any session state of that subject. A D-Bus-activated
user service lives under the user manager, outside any login session scope,
so its subject is inactive; stock inactive defaults deny the action, and a
QindaQt polkit policy file that broadened callers' rights is forbidden by
the platform plan's rule that a proxy must not gain special rules [B].
Passing interactive=true cannot repair a policy denial for an
out-of-session subject.

Therefore: the shell's session-action controller is the single caller. It
is in the session scope (supervisor-launched session process [R session
supervisor model]), owns the confirmation dialog, renders the bounded
inhibitor list from Power1's snapshot, maps the Can* result truthfully
(inhibited is not denied; authentication-required surfaces a challenge,
never a silent retry), and performs exactly one logind call per confirmed
user intent. Power1 publishes availability and inhibitor truth so the UI
has one source, but the irreversible verbs never flow through it. This is
the same session-versus-system authority separation used for the
notification privacy gate: authority follows the process boundary that
owns it [B].

Rejected alternative: make Power1 a session-scope supervisor-essential
child so its subject is active. Rejected because it widens the essential
child set (a supervisor contract change [R: exactly two essential children
today]), couples session lifetime to a service that has no other need for
it, and still leaves Power1 holding irreversible verbs that no service
needs to hold when the shell is present. Recorded as the fallback if
qualification proves the shell-subject assumption wrong on a target
platform; that proof belongs to the physical lane (section 11).

## 8. Resolving the accepted Display decision's class-B condition

The accepted Display decision made class-B brightness/color fields
provisional until the owning platform lanes confirm the KWin configuration
transport preserves device error truth and does not collapse DDC-CI, ICC,
or ambient-policy authority into Display1 [B]. This analysis resolves the
brightness share of that condition as follows:

- Device error truth: the pinned management XML carries a typed
  failure-reason event emitted before the failed event [U], and the
  transaction model treats definitive rejection as a first-class outcome
  [B D1]. The remaining uncertainty is whether a *hardware-level* DDC/CI or
  backlight failure at apply time surfaces through that reason or is
  swallowed by the compositor. That is a runtime question; the brightness
  lane's confirmation is therefore an evidence row, not prose: one
  private-bus fake-compositor row proving the class-B rejection path maps
  failure reasons into per-device degraded states, plus one physical-lane
  row on a DDC/CI monitor proving a refused write surfaces as a typed
  per-device degraded status in the brightness model. Until that physical
  row exists, the nested and fake evidence is labelled model evidence only
  [B D1 evidence-labeling rule].
- Non-collapse: DDC/CI *policy* (whether the user permits the compositor to
  drive the monitor's own controls) is a user preference surfaced as the
  class-B ddc-ci-allowed flag; the *transport* is the compositor's; the
  *authority* over whether a given monitor may be driven stays with the
  per-device capability truth, never with QindaQt guessing. ICC/HDR/color
  fields are untouched by the brightness lane entirely: the pure model
  ignores them, so no color authority leaks into brightness and vice versa.
  Ambient policy is QindaQt's (section 6), with the compositor's
  auto-brightness flag explicitly off on driven outputs — ambient authority
  therefore does not collapse into Display1 either; Display1 remains a
  typed transport.
- The brightness lane consequently has a hard ordering constraint: its
  first display-dependent slice waits for the accepted D2 service and the
  typed class-B method the display lane schedules as its class-B policy API
  [B Fable slice D7; D2 in progress by the display lead]. Before that,
  brightness work is confined to the pure model fed by fixture snapshots.

## 9. Multi-monitor identity interaction with Display1

- Persistent per-display brightness context keys off the display lane's
  stable identity precedence (EDID identifier hash, raw-EDID hash, MST
  composite, connector fallback) [B D1 contracts]. The brightness model
  never derives its own identity from connector names alone, never stores
  one, and treats the ambiguous-duplicate marker as "no persistent
  context" (sliders work; nothing persists).
- Hotplug: display snapshots carry the class-B truth per output; the model
  rebuilds its entries from each snapshot generation, so hotplug adds,
  removes, and re-binds entries without Power1 or the model holding device
  leases. A vanished output's entry disappears; no stale value is ever
  presented as current after owner loss [B platform plan].
- Suspend/resume: the compositor re-reads hardware on resume [B Fable
  section 8]; brightness snapshots may change across a cycle. The model
  treats any post-resume snapshot as fresh truth; the ambient loop resumes
  its claim on PrepareForSleep(false) and re-reads state before its next
  adjustment, with no revision on identical values [B no-op rule].
- One physical output, one slider: internal panels that also expose a DDC
  path are deduplicated by the display lane's identity; the model never
  invents a second entry, and the keyboard backlight is a different device
  class, never merged into a display entry.

## 10. Settings1, persistence, and consumers

- v1 adds no Settings1 keys and no schema migration. Reserved keys for the
  later Settings-owned slice (proposed names, bounded values, owner
  Power1/brightness model): ambient enabled toggle, ambient curve bounds
  (min/max percent clamps), keyboard-backlight idle timeout (off in v1),
  per-display restore-on-login for brightness (off in v1), lid policy
  (reserved, upstream-defaults only). All are ordinary single-key commits;
  no cross-key invariant is introduced, matching the accepted Settings1
  usage pattern [B].
- Consumers: the shell quick-settings surface gets a narrow facade (battery
  percentage and state text, profile selector, brightness sliders) fed by
  the pure models; the Settings Center Power and Brightness pages wait for
  the accepted AppShell/route registry like every other provider page [B
  platform plan scheduling]; keyboard brightness and monitor brightness
  keys register through the shell's accepted GlobalAccel adapter [B
  ADR-0009]; accessibility requires percentage-plus-text, never icon-only
  state, keyboard steps, and announced errors [B].

## 11. Verification plan

Deterministic tiers (never reported as hardware evidence [B]):

- Unit, `qindaqt.power-protocol-*`: every bound, round-trip, stable
  encoding, hostile-string sanitization (NUL, control, oversize, mixed
  lineage), enum and NaN/infinity rejection, no partial replacement of a
  caller's prior value.
- Unit, `qindaqt.power-aggregation-*`: single battery, dual battery, UPS
  plus battery, absent composite, unknown-percentage coarse-level devices,
  charging-rate sign truth, warning-level mapping, time estimate passthrough
  (never recomputed).
- Unit, `qindaqt.brightness-model-*`: dedup one-slider-per-output, mirror
  follows source, capability loss removes only that entry, percentage
  conversion via per-device min/max, curve hysteresis and clamp math with
  injected policy, keyboard rows from power values.
- Private-bus service rows, `qindaqt.power-service-*`: a private bus hosts
  fake UPower (display device, batteries, keyboard backlight, add/remove),
  fake power-profiles-daemon (profiles, holds, degradation), and a fake
  logind (Can* answers, Inhibit descriptor lifecycle, PrepareForSleep true/
  false sequences, ListInhibitors, preparing properties). Rows: activation
  and exact-owner replacement with epoch turnover; UPower replacement
  mid-operation leaving in-flight results uncertain with no replay; PPD
  absent degradation; logind absent whole-service degradation; multiple
  PrepareForSleep cycles; inhibitor descriptor closed on cancel and on
  process exit; oversized/malformed upstream properties rejected
  atomically; bus-loss exit.
- Private-bus client rows, `qindaqt.power-client-*`: debounce/coalescing,
  timeout and backoff, stale replies ignored, uncertain completion exactly
  once, stop-cancel semantics, availability mapping [B Audio client
  precedent].
- Ambient rows (fake sensor port and fake display port): lux step changes,
  hysteresis, clamp, lock pause, suspend pause/resume, sensor loss stops
  with last applied value, class-B rejection backs off, manual override
  disables the session toggle, compositor auto-brightness flag conflict
  refuses to run, claim released when disabled.
- Class-B transport model rows: fake compositor on the private bus
  proving failure-reason mapping to per-device degraded states (section 8).
- Packaging rows: staged install contains the exact executable, activation
  file, interface XML, and hardened unit; daemon-loss/replacement cycles on
  a private bus prove distinct owner/PID/epoch replacement with zero
  surviving state [B Audio packaging precedent].

Physical hardware matrix (release lane, separately reported): laptop
single/multi battery and UPS aggregates; AC plug/unplug truth; suspend/
resume with brightness and DDC state across the cycle; internal backlight
firmware/platform/raw type coverage; external DDC/CI monitors including
refused-write error truth and non-DDC monitors presenting honest
unavailable state; keyboard backlight internal-change (hotkey) events
surfacing; power profiles with real platform drivers and with the daemon
absent; the shell-as-caller polkit subject proof on a real login session
(decision 3's fallback trigger); lid close/open with and without external
outputs against logind defaults; scheduled shutdown reserved row.

Performance and footprint: measure idle PSS, private dirty memory, and
wakeups of the resident service before recording any ceiling (the Display
lane's precedent of rejecting invented fixed budgets [B]); required
properties are zero periodic timers when idle and idle, claimed sensor
released when adaptive is off, and event-driven only operation. The
session-wide 500 MiB aggregate idle budget [R wiki product constraints]
remains the frame of reference; the power service's measured share is
recorded, not assumed.

## 12. ADR topics (numbers reserved by the manager)

1. Power1 split-upstream-authority service: UPower plus optional
   power-profiles-daemon plus logind state composed into one typed epoch/
   revision boundary; no second battery estimate; PPD absence as supported
   degradation.
2. Session power actions are shell-invoked: polkit subject reasoning, the
   no-replay rule for irreversible actions, and Power1 as availability
   authority only (amends the platform plan's section B placement).
3. Brightness routing without a Brightness1 process: class-B compositor
   transport for displays, UPower transport for keyboard backlight, pure
   consumer-side model, QindaQt-owned ambient policy with the
   never-both auto-brightness guard, and the evidence path that lifts the
   accepted Display decision's class-B provisional condition.
4. Reserved, only if the lid-override slice proceeds: a QindaQt lid policy
   slice taking the block-mode lid inhibitor and its interaction with the
   display lane's internal-panel behavior and the no-KWin-patch rule.

## 13. Risks and bounded open questions

1. The shell-subject polkit assumption (section 7) is mechanism-correct per
   the documented policy model but must be proven on a real login session;
   the fallback (session-scope Power1) is recorded if it fails.
2. Where KWin 6.6.5 implements DDC/CI internally was not pinned this run
   (no ddcutil linkage, no ddc backend directory [U]); it is irrelevant to
   QindaQt's boundary because the transport is the protocol, but it decides
   whether the capability bit ever appears on real hardware and belongs to
   the display lane's physical qualification, not to QindaQt guessing.
3. Whether the nested virtual backend serves any brightness capability is
   expected negative (the accepted analysis records the virtual backend
   honors configuration fields but not brightness [B]); class-B nested rows
   therefore prove protocol modeling, never transport, and are labelled so.
4. Manual brightness keys at the lock screen: a locker/compositor
   capability question; raised for the display/shell lanes, not decided
   here.
5. iio-sensor-proxy's own documentation site was unreachable this run
   (anti-bot gate); the interface facts cited are from the compositor's
   in-tree vendored interface XML [U], which is authoritative for the wire
   contract; the implementing slice should vendor and checksum the same XML
   rather than trusting a second-hand copy.
6. Charge thresholds (EnableChargeThreshold and related properties [U]) are
   deliberately excluded from v1; they are a bounded later slice with real
   hardware variance.

## 14. Peer questions

To Rhea Calder and the display lane: does the D2/D7 schedule place the
typed class-B policy method such that the brightness lane's first
display-dependent slice can follow the accepted D2 candidate, and do you
accept the section 8 evidence path as the brightness share of lifting the
class-B provisional condition? The ambient never-both guard needs one
commit from your side: a snapshot field exposing the compositor
auto-brightness flag per output, which the pinned XML already carries [U].

To the manager: the ADR topics in section 12 need numbers; the
section 7 amendment to the earlier platform plan's Power1 action placement
needs your routing acceptance before any Power slice is assigned; and the
shell-lane session-action controller and idle-hint duty in section 10 need
a shell-lane assignment slot (they are shell-owned paths, not power-lane
paths).

To the Settings lane (Ada Ruiz): section 10's reserved keys are proposed
for a later Settings-owned schema slice; no platform worker will touch
Settings paths.

To the native-app lane: the Power and Brightness page states should mirror
the degraded-state vocabulary already coordinated for Displays
(unavailable, degraded with reason, uncertain results never replayed); no
new state names are proposed.

## 15. Sources read this session

Repository: module boundaries, Settings1 and Audio1 reference pages, audio
service module and packaging layout, testing harness, implementation
roadmap, task list, handoff, settings schema directory. Board: all files
listed in the header. Upstream primary sources, all read during this run:
the systemd login1 D-Bus API man page source (Manager and Session
interfaces: manager power methods and flags, Can* result vocabulary,
Inhibit, PrepareForSleep/PrepareForShutdown, ScheduleShutdown, lid/dock/
external-power and handle-* properties; Session SetBrightness with the
backlight and leds subsystems and driver-defined ranges, Lock/Unlock,
LockedHint, idle hints), the systemd inhibitor-locks document (lock types,
block/delay/block-weak modes, descriptor lifecycle, desktop integration
pattern, per-action inhibit polkit names, non-interactive policy checks),
the systemd writing-desktop-environments document (logind calls from the
DE, key/lid handling locks, inhibitor acknowledgment UX, SetIdleHint
duty), the UPower reference (daemon interface: enumerate, display device
contract, critical action, device added/removed, on-battery and lid
properties; device interface: full property set including type taxonomy,
state, warning and battery levels, energy and time estimates, charge
thresholds, vendor/model/serial privacy surface; keyboard-backlight
interface: get/set/max, native path, changed and changed-with-source
signals, device-name object paths), the power-profiles-daemon API
reference (ActiveProfile, Profiles taxonomy and ordering, Performance-
Degraded reasons, HoldProfile/ReleaseProfile cookies and ProfileReleased,
Actions and ActionsInfo, BatteryAware), the kernel sysfs-class-backlight
ABI document (brightness, actual_brightness, max_brightness, bl_power,
type semantics and the firmware-over-platform-over-raw preference), the
pinned plasma-wayland-protocols v1.20.0 output-device and output-management
XMLs (capability bits including brightness, ddc-ci, auto-brightness;
brightness metadata and overrides in nits; brightness, dimming, sdr
brightness in fixed scales; ddc-ci-allowed; set requests; failure-reason
before failed since version 12; replication source; priority), and the
KWin v6.6.5 build files and source tree listings on the tagged mirror
(no ddcutil dependency anywhere, backends limited to drm, fakeinput,
libinput, virtual, wayland, x11; in-tree consumption of the sensor-proxy
interface XML). Access limitations recorded: freedesktop.org man pages and
the iio-sensor-proxy GitLab repository were behind anti-bot gates this run;
the login1 facts were read from the systemd project's own source mirror of
the man page, and the sensor-proxy interface facts from KWin's vendored
copy.

Recap: Power1 is one bounded resident service owning observation, profiles,
keyboard backlight, and the ambient loop; Brightness1 is deliberately not a
process because the compositor is already the single transport for display
brightness and a second writer would fight Display1's lineage. The one
material amendment to the earlier platform plan — session actions move to
the shell as the polkit-visible session subject — is argued from the
documented logind authorization model and carries a recorded fallback. The
slice order keeps every display-dependent row gated on the accepted D2
candidate, keeps all fake-bus and fake-sensor rows deterministic, and
separates physical truth into its own lane. No product file was touched by
this analysis.
