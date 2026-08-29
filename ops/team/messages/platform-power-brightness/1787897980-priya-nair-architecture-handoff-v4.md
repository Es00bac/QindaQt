# Power1 and Brightness platform architecture and slice order — replacement handoff (v4, exact-review repair)

- Worker: Priya Nair, QindaQt Power and Brightness Platform Architect
- Provider/model: GLM, exact model `zai-coding-plan/glm-5.3-flash`, reasoning
  variant high; analysis and planning only
- Timestamp: 2026-08-28T06:20:30Z start of writing; terminal post of this run
- To: Dorian Vale (exact rereview request); Elara Finch (prior reviewer,
  FYI); Rhea Calder, receiving platform/display lead; Kellan Ward, D1 lead;
  QindaQt manager (routing)
- Supersedes: `1787896208-priya-nair-architecture-handoff-v3.md` (exact
  SHA-256 `23e6a3e5880410858871073549089a8c45d6a381bee7bd9f4cb8cc8c4adc68e2`,
  977 lines), which is WITHDRAWN as a candidate. This v4 is the single
  replacement revision offered for exact rereview. It repairs exactly the
  four findings in Dorian Vale's verdict `1787897128` (P1-A, P1-B, P1-C,
  P2-A; P0/P1/P2/P3 = 0/3/1/0, verdict FAIL for QQ-005.03 MODELLED
  integration) and carries every accepted v3 decision forward unchanged
  except where a finding requires integration (§1.2 section map).
- Prior repairs inherited: Elara Finch FAIL verdict `1787893500`
  (P0-1, P1-2…P1-8, P2-9…P2-14, P3-15…P3-22) with midpoint
  `1787892600` (F1–F6) and Kellan Ward's D1 boundary help `1787891463`
  (all 22 numbered dispositions from v2 §1 remain binding); Dorian Vale's
  v2-review verdict `1787895220` (six findings, closed in v3; the four
  dispositions his v3 verdict marks PASS — P1-2, P2-4, P3-5, P3-6 — are
  not reopened here); Dorian Vale's v3 exact-review FAIL `1787897128`
  (P1-A, P1-B, P1-C, P2-A) repaired in this v4 (§1.0).
- Evidence identity: read-only detached worktree
  `/home/cabewse/work_SPaC3/container-wm-workers/power-brightness-analysis`
  re-verified this session at exact base
  `94e84077e33a279dcebee24511e7dbdf1b87e3e1` (git rev-parse + clean
  status). The manager integration tree
  `/home/cabewse/work_SPaC3/container-wm-workers/qst1-manager-integration`
  was read read-only in the v3 session for two authority checks: ADR-0015
  (§10/§11 grounding for P3-5) and the compositor-session page plus
  supervisor source (§9 grounding for P1-3); the four v4 repairs require
  no new product-tree reads beyond this session's base re-verification.
  No product edit, Git command, build, test, UI/session/display run, or
  host D-Bus/logind/power/battery/backlight/DDC-I2C/inhibitor/session/
  display/settings/hardware access at any point in any run.
- Upstream fact discipline: no upstream fetch was attempted this run and
  none succeeded or is cited as newly fetched. The four v4 repairs ground
  in Dorian Vale's pinned primary-source citations carried through his
  verdict `1787897128`: KWin v6.6.5
  `src/backends/drm/drm_connector.cpp:202-205` (`isInternal()` true
  exactly for LVDS, eDP, or DSI), systemd-257
  `org.freedesktop.systemd1.Manager.SetEnvironment(as)` (with
  `UnsetAndSetEnvironment(as,as)`) and the absence of any
  `UpdateActivationEnvironment` on systemd
  (`freedesktop.org/software/systemd/man/257/org.freedesktop.systemd1.html`),
  and the D-Bus specification's
  `org.freedesktop.DBus.UpdateActivationEnvironment(a{ss})`
  (`dbus.freedesktop.org/doc/dbus-specification.html#bus-messages-update-activation-environment`).
  All other upstream protocol and KWin facts remain pinned [U] through
  reviewer board records: Dorian Vale `1787895220`/`1787897128` (KWin
  v6.6.5 `src/workspace.cpp:1452-1494` matching semantics;
  `kde-external-brightness-v1.xml:58-74` external-change contract;
  `set_edid` = base64 of the first 128 EDID bytes) and Elara Finch
  `1787893500`/`1787892600` (logind, systemd-257, protocol, and KWin
  pins), plus Kellan Ward `1787891463`.
- Evidence legend: **[R]** repository fact re-verified this session at the
  exact base or in the read-only integration tree (path cited) · **[U]**
  upstream primary-source fact pinned through the cited board records ·
  **[B]** other accepted board record · **[I]** inference, labelled, not
  evidence · **[P]** proposed contract subject to lead/manager acceptance ·
  **[Q-det]** deterministic evidence tier · **[Q-hw]** physical
  qualification tier, never implied by Q-det.
- Limits observed: board writes only (this thread and
  `ops/team/workers/priya-nair.md`); no product source, test, doc, build,
  task-list, handoff, or Git state touched.

## 1. Disposition of the exact review findings

Disposition vocabulary: **accepted** = the finding stands and the decision
changes here; **accepted-tightened** = accepted with a stricter contract
than the minimal repair. No finding is rejected, in any round.

### 1.0 Disposition of the exact v3 review findings (Dorian Vale `1787897128`; new in v4)

| Finding | Subject | Disposition | Resolved in |
| --- | --- | --- | --- |
| P1-A | Identity gate excludes DSI, which KWin 6.6.5 classifies as internal; one-eDP-plus-one-DSI rig binds the wrong panel | accepted-tightened | §2 decision 11, §4.1 `discover()`, §8.2 pinned set + G2, §11 rows R12/R15/R16/R19/R35 |
| P1-B | systemd exposes `Manager.SetEnvironment(as)`, not `UpdateActivationEnvironment`; S2 as written fails before activation | accepted | §9 S2/S3, §10, §11 row R36 |
| P1-C | S8 takeover × S6 exit-generation × S9 shared environment let two same-user supervisors stop/restart Power1 forever | accepted-tightened | §2 decision 13, §9 S6/S8/S9, §10, §11 rows R32/R34 |
| P2-A | S4 "exactly one activation attempt" contradicts S6/R33 "up to three" in the same generation | accepted | §9 S4/S6, §10, §11 row R33 |

Repair notes:

- **P1-A** is real: v3 G2's `eDP*`/`LVDS*` class would register on the
  verdict's minimal counterexample (one connected eDP panel + one
  connected DSI panel + one eligible backlight) and KWin could bind the
  device to the wrong panel under the already accepted flag-only,
  iteration-order semantics — the exact fail-open P1-1 forbids. v4 takes
  both offered repairs: G2 counts the pinned adapter's internal set
  (LVDS, eDP, DSI), and the gate consumes a typed injected inventory
  whose `internal` classification is derived from that pinned set rather
  than a bare filename heuristic (§4.1). The one-DSI and mixed-eDP/DSI
  cases are named inputs of rows R12, R15, R16, R19, and R35.
- **P1-B** is real: v3 S2 named `UpdateActivationEnvironment` on both
  endpoints; systemd has no such method. v4 S2 names the exact calls and
  signatures per endpoint, requires both replies awaited before S4, adds
  a publication-failure row to §10, and adds the fake-manager
  signature/error row R36. A helper executable may issue the two calls;
  the service-manager call remains `SetEnvironment`.
- **P1-C** is real and the v3 multi-session rule is replaced, not
  tightened: arbitration is now deterministic single-session selection
  from shared read-only logind truth (§9 S8). A losing supervisor
  publishes typed unavailable and never stops the winner, never
  publishes, never activates, and never retries; only the deterministic
  winner may activate or take over, so the verdict's
  A-stops-B/B-stops-A trace is unreachable by construction. R34 is
  rewritten as the simultaneous-two-supervisor row proving convergence
  and zero stop/restart cycles, plus an arbitration-flip phase.
- **P2-A** is real wording debt: v4 states one count everywhere — the
  generation budget is **one initial attempt plus at most two retries**
  (three attempts total, 1 s/2 s/4 s backoff) — applied identically to
  S4, S6, publication retries, and row R33.

### 1.1 Disposition of the exact v2 review findings (carried from v3, binding)

Every v2 disposition of Elara's findings remains binding and is not
repeated here. v3's six dispositions of Dorian's v2-review findings were
accepted in v3 §1; v4 re-verifies that the four his v3 verdict marks PASS
carry forward byte-for-byte untouched.

| Finding | Subject | Disposition | Resolved in |
| --- | --- | --- | --- |
| P1-1 | Internal-panel matching rule false under KWin 6.6.5 EDID omission; can bind the wrong panel | accepted-tightened | §2 decision 11, §8.2, §11 rows R12–R19 (v4: DSI amendment, §1.0) |
| P1-2 | Provider port cannot satisfy the protocol's external-change contract | accepted-tightened | §2 decision 12, §8.5, §11 rows R20–R26 |
| P1-3 | Correct Wayland environment gated but deterministic activation absent | accepted-tightened | §2 decision 13, §9, §11 rows R27–R34 (v4: arbitration/API/count amendments, §1.0) |
| P2-4 | Destructive-key safety after partial inhibitor acquisition/loss underspecified | accepted | §7.4, §11 rows R7–R11 |
| P3-5 | 500 MiB performance reference contradicts accepted ADR-0015 | accepted | §2 decision 14, §11 physical matrix |
| P3-6 | PB-0 declared test path disagrees with its own module map | accepted | §2 decision 15, §12 PB-0/PB-1 rows |

P2-4 does not change shell ownership of the `handle-*` inhibitors (v2
decision 3 stands); it adds the atomic-acquisition and loss contract in
§7.4, so it is recorded as a §7/§13 contract addition rather than a new
executive decision.

### 1.2 Carry-forward section maps

v3 → v4 (every change in this revision; all unlisted content is carried
verbatim):

| v3 section | v4 section | Change |
| --- | --- | --- |
| header | header | v4 identity; supersedes v3; inherited-repairs and upstream-discipline bullets updated |
| §1 | §1.0–§1.2 | new v3-review disposition table; v2 table carried; maps restructured |
| §2 decisions 1–10 | §2 | carried verbatim |
| §2 decision 11 | §2 | internal class = pinned KWin set incl. DSI (P1-A) |
| §2 decision 13 | §2 | arbitration + unified attempt count (P1-C, P2-A) |
| §2 decisions 12, 14, 15 | §2 | carried verbatim |
| §3 authority map | §3 | carried verbatim |
| §4 module table/rules | §4 | carried verbatim |
| §4.1 `discover()` | §4.1 | connector `internal` classification derived from the pinned adapter set (P1-A) |
| §5 wire contract | §5 | carried verbatim |
| §6 brightness contract | §6 | carried verbatim |
| §7 session-action boundary | §7 | carried verbatim |
| §8 provider design | §8 | §8.2 pinned set + G2 amended for DSI (P1-A); §8.1/8.3–8.6 carried |
| §9 S1, S3, S5, S7, S10 | §9 | carried verbatim |
| §9 S2 | §9 | exact `SetEnvironment(as)` / `UpdateActivationEnvironment(a{ss})`, awaited replies (P1-B) |
| §9 S4, S6 | §9 | unified one-initial-plus-two-retries count (P2-A) |
| §9 S8, S9 | §9 | deterministic single-session arbitration; winner-only takeover; loser typed unavailable (P1-C) |
| §10 failure policy | §10 | publication-failure and arbitration rows added; wrong-lineage and retry rows amended |
| §11 rows R1–R11 | §11 | carried verbatim |
| §11 rows R12/R15/R16/R19 | §11 | one-DSI and mixed-eDP/DSI counterexamples; pinned-set naming (P1-A) |
| §11 rows R13/R14/R17/R18/R20–R31 | §11 | carried verbatim |
| §11 rows R32/R33/R34/R35 | §11 | winner-only takeover; count wording; simultaneous-arbitration row; DSI in fake semantics |
| — | §11 row R36 | new fake-manager signature/error row (P1-B) |
| §12 slice order | §12 | PB-2 evidence range R12–R36; rest carried |
| §13 ADR topics | §13 | topic 3 extended with arbitration + exact call signatures |
| §14 open items | §14 | items 4–5 amended; item 11 added |
| §15 rereview request | §15 | rewritten for the four v4 repairs |
| §16 non-claims | §16 | naming list updated; run notes updated |
| §17 self-identity | §17 | v4 path and self-hash |
| recap | end | updated |

v2 → v3 map (historical, carried verbatim from v3):

| v2 section | v3 section | Change |
| --- | --- | --- |
| §1 dispositions (22 findings) | carried, binding | none |
| §2 decisions 1–10 | §2 decisions 1–10 | 2 and 9 amended; 10 unchanged |
| §3 authority map | §3 | internal-panel row, new activation row |
| §4 decomposition/rules | §4 | port redefined discovery + apply + observe; explicit interface block |
| §5 wire contract | §5 | `wayland_binding`, provider reason codes added |
| §6 brightness contract | §6 | capability-truth wording tied to §8.2 gate |
| §7 session-action boundary | §7 | 7.3 carried; new 7.4 (P2-4) |
| §8 provider design | §8 | 8.2 rewritten fail-closed; 8.5 new (P1-2) |
| §9 verification plan | §11 | ordered numbered rows; P2-11 discipline kept |
| §10 slice order | §12 | PB-0/PB-1 explicit paths; PB-2 gate replaced |
| §11 ADR topics | §13 | topic 3 extended |
| §12 open items | §14 | activation open item closed; additions labelled |
| §13 peer questions | §15 | rereview addressee is now Dorian Vale |
| §14 non-claims | §16 | carried |
| — | §10 failure policy | new, consolidates fail-closed rules |

## 2. Executive decisions (v4; 1–15 carried from v3 — 11 and 13 amended in place for P1-A/P1-C/P2-A, the rest verbatim)

1. **One bounded resident service `org.qindaqt.Power1` exists; a
   Brightness1 process does not.** Carried verbatim from v2 (§2.1): the
   provider is a cohesive collaborator inside Power1; display brightness
   composition/presentation remains a pure consumer-side model; one
   epoch/revision authority per domain.
2. **The backlight provider lives in the Power1 process** [P, carried with
   one amendment]: a `kde_external_brightness_v1` v3 Wayland client that
   registers the internal panel **only under the §8.2 proved one-to-one
   identity gate** (amendment for P1-1; v2's "register and let KWin match"
   fallback for ambiguous rigs is withdrawn as fail-open), applies each
   `requested_brightness` through logind
   `Session.SetBrightness("backlight", <device>, <value>)` (capability
   check, not polkit [U]), and commits `set_observed_brightness` on
   external change (§8.5). Discovery reads
   `/sys/class/backlight/*/{type,max_brightness,actual_brightness}`,
   `/sys/class/drm/*/status`, and `/sys/class/drm/*/edid` strictly
   read-only; QindaQt never writes sysfs and never opens `/dev/i2c*`.
   DDC/CI stays honest-unavailable in v1 (PB-6 reserved). Capability truth
   for display brightness = provider registration under the gate:
   compositor requests; QindaQt provider applies via logind.
3. **Session power actions are invoked by the shell, and the shell alone
   owns their authorization truth.** Carried from v2 (§2.3): no `Can*`
   field in Power1; the shell controller calls `Can*` itself and maps all
   seven systemd-257 values [U]; the same slice takes the three
   `handle-*` inhibitors and owns key confirmation — now additionally
   bound by the §7.4 all-or-nothing and loss contract (P2-4).
4. **QindaQt owns no ambient/adaptive authority.** Carried verbatim (v2
   §2.4): compositor `auto_brightness` is the sole adaptive authority;
   KWin observes user adjustments to learn its curve [U] — the exact fact
   that forces §8.5's commit-don't-overwrite choice for external change.
5. **Lock-before-sleep belongs to KWin/KScreenLocker.** Carried verbatim
   (v2 §2.5).
6. **Inhibitor rule for Power1: delay-only, and v1 holds none.** Carried
   verbatim (v2 §2.6); lid override reserved to a shell/compositor slice.
7. **The brightness model is independent of the private D1
   implementation.** Carried verbatim (v2 §2.7, §6.2 fixture boundary);
   D0/D1 unintegrated, D2 unassigned, typed class-B is D7.
8. **Idle hints are Power1's duty; auto-suspend stays logind's.** Carried
   verbatim (v2 §2.8): `ext_idle_notification_v1` consumer +
   `SetIdleHint` on the display session.
9. **Lifetime and activation environment — amended and closed (P1-3).**
   v2 carried: Power1 exits on permanent session-bus loss **and**
   compositor (Wayland) connection loss; restart re-registers fresh, no
   retained handles. Amended: the open "mechanism/owner is a manager
   decision" item is replaced by decision 13 below. The unit remains
   D-Bus-activated only — **no** `WantedBy=` dependency (contrast Audio1
   [R `src/services/audio_service/data/qindaqt-audio-service.service.in`]),
   so nothing auto-starts before the session lane's publication gate.
10. **v1 stores no power or brightness state in Settings1.** Carried
    verbatim (v2 §2.10).
11. **Fail-closed internal-panel identity (P1-1; amended v4, P1-A).** The
    provider registers an internal brightness device only when a proved one-to-one
    rule holds — exactly one eligible backlight after the kernel's
    firmware > platform > raw preference and exactly one connected
    internal-class connector, where the internal class is KWin 6.6.5's exact
    `DrmConnector::isInternal()` set — kernel naming classes `LVDS*`, `eDP*`,
    and **`DSI*`** [U `1787897128`] (§8.2 G2). Any ambiguous or multi-internal
    topology — including the verdict's one-eDP-plus-one-DSI rig — registers
    **none** and publishes typed unavailable truth. EDID cannot
    disambiguate KWin's internal path and is never a matching input
    (§8.2).
12. **The provider observes external brightness change (new, P1-2).**
    The port is discovery + apply + **observe**. When hardware or
    firmware changes brightness, the provider updates the Power1 snapshot
    and commits `set_observed_brightness` — never the protocol's
    overwrite alternative, which would defeat KWin's observation-based
    adaptive learning (decision 4) [U `1787895220` lines 58-74]. The
    production source is event-driven where reliable with a bounded
    measured fallback (§8.5).
13. **Deterministic activation under single-session arbitration: the
    session supervisor publishes the child socket and unconditionally
    activates Power1 per published generation — one initial attempt plus
    at most two retries — with only the deterministic arbitration winner
    ever activating or taking over (P1-3; amended v4, P1-C/P2-A; v3's
    "exactly one attempt" wording and last-writer-wins multi-session rule
    are withdrawn).** Owner: the
    `qindaqt-session` session supervisor — the session-lane process KWin
    launches to control compositor lifetime and start exactly the
    notification host and shell [R `docs/wiki/architecture/compositor-session.md:63-74`
    in the integration tree; `src/session_supervisor/src/session_process_supervisor.cpp:70-97`].
    Full sequence, arbitration, proof, wrong-lineage, and multi-session
    behavior: §9.
14. **The session-wide resource frame of reference is the 1,024 MiB
    initial aggregate idle PSS ceiling (new, P3-5).** ADR-0015 supersedes
    the 500 MiB bring-up target; 1,024 MiB is a ceiling, not a
    consumption goal; measurements are recorded first and tighter budgets
    come only from a later superseding decision [R ADR-0015 read this
    session; Dorian's citation of
    `docs/wiki/development/testing-harness.md:781-785`].
15. **All test paths are written out brace-free and match the module
    table (new, P3-6).** PB-0 uses exactly the two roots that mirror its
    two modules — `tests/services/power_protocol/` and
    `tests/services/brightness_model/` — as three independently
    reviewable commits; v2's `power_{protocol,brightness_model}` brace
    expression (expanding to nonexistent `power_brightness_model`) and
    PB-1's `power_{service,client}` brace expression are removed (§12).

## 3. Domain authority map (v3)

| Concern | Authority | Observation path | Mutation path |
| --- | --- | --- | --- |
| Battery, AC, UPS state and estimates | UPower daemon [U] | Power1 typed snapshot | none in v1 (charge thresholds reserved) |
| Power profiles | power-profiles-daemon, optional [U] | Power1 snapshot | Power1 SetProfile and profile holds (§5.3 carried) |
| Suspend/hibernate/reboot/power-off | systemd-logind [U] | Power1 mirror of `PreparingForSleep` only | shell session-action controller (decision 3) |
| Session-action authorization truth (`Can*`) | logind, caller-relative [U] | none in Power1 | shell controller queries and maps all seven values |
| Power/suspend/hibernate keys | logind `handle-*` + in-session inhibitors [U] | none in Power1 | shell controller inhibitors + confirmation under §7.4 all-or-nothing |
| Inhibitors | logind inhibitor locks [U] | Power1 bounded sanitized list, no `uid`/`pid`; own-state | Power1: none in v1; shell `handle-*` per §7.4; Display1 preview delay inhibitor [B]; lid block reserved shell/compositor |
| Lid | logind lid properties + UPower [U] | Power1 snapshot | none in v1; override reserved shell/compositor |
| Idle hints | DE duty per integration guide [U]; implemented by Power1 | Power1 system idle truth | Power1 `SetIdleHint` only |
| Internal-panel brightness | compositor requests; QindaQt provider applies via logind [U] | Power1 provider device snapshot: registered device, observed value, typed status with identity reason codes (§5) | provider apply of compositor `requested_brightness` |
| **External brightness change (hw/firmware origin)** | **provider observes and reports (P1-2)** | **Power1 snapshot last-observed update** | **provider `set_observed_brightness` commit; compositor stays the adaptive learner [U]** |
| Provider↔compositor binding | session supervisor publication + Power1 gate (P1-3) | Power1 `wayland_binding` snapshot field (§5) | session supervisor publication/activation (§9) |
| External-monitor brightness | none in v1 — honest unavailable | Power1 snapshot shows no registered device | reserved PB-6 transport decision |
| Display brightness values pre-D7 | provider snapshot (hardware truth) | Power1 snapshot | provider |
| Display brightness values post-D7 | compositor class-B values via D7 [B `1787891463`] | display client (PB-5 only) | display client typed method |
| Adaptive brightness | compositor `auto_brightness` [U] | closed class-B policy value via D7 (PB-4/PB-5) | compositor; QindaQt none |
| Keyboard backlight | UPower keyboard-backlight interface [U] | Power1 snapshot, singleton-deduped | Power1 typed set with result lineage |
| Ambient light readings | sensor-proxy, claimed by KWin [U] | none in Power1 | none |

Session-versus-system separation restated (carried): everything a user
service may do without privilege or session scope is Power1's; everything
requiring the session subject for polkit or key handling is the shell's or
KWin's. New in v3: the child-socket publication duty belongs to the
session supervisor because it is the session-lane process that provably
exists after the child compositor does [R §9].

## 4. Service decomposition, module boundaries, and interfaces (v3)

Module table is carried from v2 §4 with one change: the provider's
injected test port is redefined.

| Module | Responsibility | Allowed inward dependencies | Forbidden |
| --- | --- | --- | --- |
| `src/services/power_protocol` | Fixed typed wire values: snapshot, device, profile, backlight device + typed status + identity reason codes, operation results, kbd backlight values; bounds; fail-closed total decoding; version/epoch/revision lineage | Qt Core/DBus | transport state, UPower/PPD/logind/Wayland objects, QML, settings |
| `src/services/power_service` | Resident orchestrator only: activation/ownership, collaborator lifetime, snapshot publication | collaborators via injected ports; power protocol | owning any transport object; QML; polkit overrides; direct Suspend calls; sysfs/i2c access |
| `src/services/power_backlight_provider` | P0-1 collaborator: external-brightness v3 client, logind `SetBrightness` apply, read-only discovery, §8.2 identity gate, §8.5 observation, per-device error truth; injected `BrightnessDevicePort` (discovery + apply + observe) | power protocol values; Wayland bindings generated from the pinned, checksummed XML; Qt Core/DBus/Wayland | power_service orchestration inside it; any display lane module; sysfs write; `/dev/i2c*`; DDC transport |
| `src/services/power_idle` | P2-9 collaborator (carried) | power protocol values; Wayland idle bindings; Qt Core/DBus | shell input; timers when idle |
| `src/services/power_client` | Owner-bound async client (carried) | power protocol, Qt Core/DBus | service implementation, QML |
| `src/services/brightness_model` | Pure composition on the fixture boundary (carried, §6) | power protocol values; fixture value types; Qt Core | D-Bus, Wayland, QML, files, real clocks, transports, curve policy |
| UpowerState, PowerProfiles, LogindState, KbdBacklight collaborators | One concern each (carried) | power protocol; their upstream interface | each other's transports; orchestration |
| Shell-side (shell lane) | Session-action controller incl. §7.4 inhibitor transaction | public power/display clients | raw protocol, service internals |
| Session-lane (session supervisor owner) | §9 publication + activation + bounded retry | existing supervisor structures | Power1 internals; composition policy |

Module rules carried (P2-14/P3-20): the orchestrator owns no transport
object; `power_service` and its tests need no Wayland; power modules never
link `display_*` modules; the brightness model links neither display nor
power transport, only values; the provider links only generated bindings
from the pinned checksummed XML, never display lane code.

### 4.1 Explicit interface contracts [P]

**`BrightnessDevicePort`** — the provider's injected seam (testable
without Wayland or sysfs; P2-11 discipline):

- `discover() -> DiscoveryResult` — pure read of the injected discovery
  view: eligible backlights (type, max, actual, native id) and
  internal-class connector inventory (names, connected flags, EDID
  bytes). The inventory's `internal` classification is derived from the
  pinned KWin 6.6.5 `DrmConnector::isInternal()` set (`LVDS*`/`eDP*`/
  `DSI*` [U `1787897128`]) — a typed injected classification, never a
  narrower filename heuristic re-derived inside the provider (v3-review
  P1-A). Total, fail-closed decoding; no partial results.
- `apply(device, value) -> ApplyResult` — one logind
  `Session.SetBrightness("backlight", <name>, <value>)` dispatch;
  lineage-stamped; errors are typed, never swallowed.
- `observe(device) -> Subscription` — an external-change subscription
  handle (P1-2). The subscription is owned by the provider collaborator,
  exists only while the device is registered, and is closed on teardown,
  disappearance, or epoch turnover. Emissions carry the observed value;
  the port guarantees at-most-once coalesced delivery per change window
  (§8.5) and reports disappearance as a typed terminal event.

**Session-lane ↔ Power1** (§9): the activation environment keys
`WAYLAND_DISPLAY` and `QINDAQT_SESSION_WAYLAND_SOCKET` (equal values,
published atomically); Power1's read-only `wayland_binding` D-Bus
property; the `org.qindaqt.Power1` name as the activation and takeover
handle.

**Shell controller internals** (§7.4): one inhibitor transaction handle
covering the three `handle-*` locks; a key-action registry with
register-all/unregister-all semantics tied to that transaction's state.

Tests mirror (explicit, brace-free): `tests/services/power_protocol/`,
`tests/services/power_service/`, `tests/services/power_client/`,
`tests/services/power_backlight_provider/`, `tests/services/power_idle/`,
`tests/services/brightness_model/`. Docs paths carried: `power-service.md`,
`power1-v1.md`, `brightness-routing.md`. Packaging carried (Audio
precedent): one interface XML, one D-Bus activation `.service.in`, one
hardened systemd user unit under `src/services/power_service/data/`;
`PrivateDevices=true` stays (provider writes via logind D-Bus, reads
sysfs read-only) [B Elara midpoint].

Process and lifetime contract for Power1 [P] (v2 §4 carried): D-Bus
activated, resident after first activation, single Qt main thread, all
upstream D-Bus asynchronous and provider dispatch on the main thread,
exits on permanent session-bus loss **and** Wayland connection loss, not
an essential supervisor child [R supervisor starts exactly the
notification host and shell], never blocks logout or shutdown, holds zero
inhibitors in v1.

## 5. Power1 wire contract (v3; carried plus additions)

### 5.1 Snapshot

Carried from v2 §5.1 in full: schema version, service epoch, monotonic
revision, availability, capability bits, bounded reason code and
diagnostic; power-source truth; composite battery; bounded extra power
supplies (≤8); power profiles (supported ≤4, holds ≤8); sanitized bounded
inhibitor summary (`who`/`why`/`what`/`mode` only, **no `uid`/`pid`**, ≤8);
keyboard-backlight devices (≤8, normalized + raw, hashed-path singleton
dedupe); provider-registered backlight devices (≤8: device name, internal
flag, backlight type, max brightness, last observed brightness, typed
status).

v3 additions:

- Provider device typed status vocabulary gains identity reason codes for
  the §8.2 gate's fail-closed outcomes: `unavailable(no-backlight)`,
  `unavailable(ambiguous-backlight)`,
  `unavailable(no-internal-connector)`,
  `unavailable(ambiguous-internal-topology)`, alongside the carried
  `ok`/`degraded(reason)`/`unavailable` and P1-8 degradation truth.
- New bounded `wayland_binding` field at snapshot level: the exact child
  socket name Power1 is bound to, the compositor's reported
  external-brightness protocol version, and the binding epoch — typed
  truth for §9's wrong-lineage and multi-session behavior (truth is never
  silent about which compositor the provider serves).

Removed-relative-to-v1 set and the epoch rule are carried verbatim: any
upstream owner replacement — UPower, PPD, logind (`NameOwnerChanged`), or
service restart — creates a new epoch and invalidates all handles;
operation results carry kind, typed status, initiating and observed
lineage, reason code, bounded diagnostic; timeout or authority loss after
dispatch is uncertain; clients resnapshot and never replay. Handles are
(epoch, upstream object path) pairs.

### 5.2 Text and privacy

Carried verbatim (bounded UTF-8, no NUL, control replacement, UTF-8
boundary truncation, hard array bounds, no `a{sv}` bags; serials and raw
sysfs paths never published; attacker-controlled inhibitor text
sanitized; `uid`/`pid` dropped, not hidden).

### 5.3 Profile holds (P2-13)

Carried verbatim from v2 §5.3 (daemon cookies as epoch-scoped values;
`ProfileReleased` typed propagation; release on client-vanish; legacy
PPD name as documented degraded provider).

## 6. Brightness domain contract (carried)

### 6.1 What the pure model owns

Carried verbatim from v2 §6.1: one control entry per physical display;
mirrors follow sources; capability truth per entry (pre-D7 =
provider-registered internal backlight from the Power1 snapshot or honest
unavailable; post-D7 class-B joins via PB-5 only); keyboard rows;
percentage math only in presentation through per-device min/max; wire
keeps exact bounded integers; coalescing; no requested value reported as
current until authoritative confirmation; ambiguous-duplicate = no
persistent context; hotplug rebuild from snapshot generations; no stale
value survives owner loss; post-resume snapshots are fresh truth with the
no-op revision rule; no curve, lux input, toggle, or override policy —
KWin observes user brightness changes and learns them into its own curve
[U] (the fact that decides §8.5's commit-not-overwrite rule).

### 6.2 The fixture boundary (P1-7, per Kellan Ward's exact help)

Carried verbatim: model binds only to the brightness-lane-owned injected
value/fixture interface keyed by opaque stable-ID strings including
`replicationSourceStableId`; D1 contributes stable identity, ambiguity,
replication topology, lineage value types, and closed `ChangeClass` only;
no brightness fields pretend membership in `Display::Output`; no identity
from connector names alone; class-B values stay outside transaction
input; no Power path includes a display protocol header before D7's
contract is accepted.

### 6.3 Mutation paths

Carried verbatim: keyboard backlight via the power client (typed result,
never auto-retried); display brightness reserved to PB-5 via D7's typed
class-B method (`Applied` only after device confirmation, else
`Unconfirmed`, P1-8); pre-D7 hardware truth flows one way from the
provider snapshot.

## 7. Session-action boundary (carried; 7.4 new)

### 7.1 Authorization mechanics

Carried verbatim from v2 §7.1: stock logind policy family
`allow_any=auth_admin_keep, allow_inactive=auth_admin_keep,
allow_active=yes`; a subject with **no login session** receives
`allow_any` [U]; `interactive=true` converts to an administrator
challenge rather than repairing a denial; a D-Bus-activated user service
is a no-session subject [I, consistent with Settings1/Audio1]; no QindaQt
polkit broadening [B].

### 7.2 The shell controller owns all authorization truth

Carried from v2 §7.2: the controller calls `CanPowerOff`/`CanReboot`/
`CanSuspend`/`CanHibernate` immediately before presenting actions and
maps all seven answer values [U]; renders the bounded inhibitor list
(Power1's sanitized snapshot, else its own unprivileged `ListInhibitors`
with identical sanitization); truthful mapping (`inhibited` ≠ denied;
`challenge` surfaces authentication, never silent retry); exactly one
logind call per confirmed intent; takes the three `handle-*` block-mode
inhibitors for the session lifetime (`allow_inactive=yes,
allow_active=yes` so the in-session shell may hold them [U]) and owns
key confirmation UX, replacing logind's unconfirmed defaults inside
QindaQt sessions — **now bound by §7.4**.

### 7.3 Inhibitor descriptor contract for every QindaQt holder (P2-12)

Carried verbatim: an inhibitor is owned by the process that took it;
closed on cancel, process exit, and bus loss; never passed across D-Bus;
a replacement process never retakes a lock without re-evaluating its
reason; delay locks released within `InhibitDelayMaxSec` (5 s default)
after `PrepareForSleep(true)` [U]; logind-restart survival unproven [I]
with its fake-logind row; binds PB-3, Display1's preview inhibitor, and
any future Power-lane delay holder.

### 7.4 All-or-nothing key-inhibitor safety (new, P2-4)

The shell controller's three `handle-*` locks and its three KGlobalAccel
hardware-key actions form one atomic unit [P]:

- **Acquisition is all-or-nothing before any key action is active.** The
  controller takes `handle-power-key`, `handle-suspend-key`, and
  `handle-hibernate-key` as one transaction. Only when all three locks
  are confirmed held are all three KGlobalAccel actions registered. A
  partial result (any lock refused or lost during acquisition) releases
  every already-acquired lock immediately and leaves zero key actions
  registered. Acquisition is retried bounded (3 attempts, 1 s/2 s/4 s
  backoff); exhausted, the controller runs degraded — zero key actions,
  logind defaults unmodified and honestly in force, degraded state
  visible in the controller's own status. This closes the review's
  hazard: logind never executes a default action while the shell also
  presents or dispatches confirmation, because confirmation UX is
  reachable only from key actions, and key actions exist only under the
  full lock set.
- **Any loss disables before logind defaults can act on the next key.**
  Loss of any kind — FD loss, bus disconnect, logind replacement (epoch
  turnover), or a verification read showing a lock absent — immediately
  unregisters **all three** key actions and re-enters acquisition. The
  guarantee is collective because the locks were acquired as one
  transaction; per-key partial coverage would silently reintroduce the
  mixed-state hazard. Re-acquisition success re-registers all three.
- **Shutdown during confirmation.** `PrepareForSleep(true)` or
  inhibitor-delay expiry while a confirmation is pending drops the
  pending confirmation without dispatching; the controller never
  dispatches a destructive action after the sleep boundary. Logind
  proceeds per its own defaults and other inhibitors.
- Loss detection is event-plus-verification: the controller listens for
  loss signals and additionally re-verifies its lock set against
  `ListInhibitors` on every epoch turnover and on a bounded slow cadence
  (2 s) while actions are registered [P; the 2 s cadence is [I] until
  R11 passes].

## 8. Backlight provider design (v3; rewritten for P1-1, extended for P1-2)

### 8.1 Structure

`src/services/power_backlight_provider/` is one cohesive collaborator in
the Power1 process with the injected `BrightnessDevicePort` (§4.1:
discovery + apply + observe) so every rule below is testable without
Wayland or sysfs; no `power_service` orchestration logic lives inside it
(P2-14).

### 8.2 Identity and registration — fail-closed under exact KWin semantics (P1-1; amended v4, P1-A)

Pinned semantics [U `1787895220`; DSI fact U `1787897128`]: KWin v6.6.5
`Workspace::assignBrightnessDevices` (`src/workspace.cpp:1452-1494`)
matches an internal output by checking only
`output->isInternal() == device->isInternal()` and returns true
immediately (lines 1469-1471); EDID comparison is used only for
non-internal outputs (line 1472); candidate devices are consumed in
device iteration order. KWin 6.6.5's `DrmConnector::isInternal()`
(`src/backends/drm/drm_connector.cpp:202-205`) returns true exactly for
connector types **LVDS, eDP, or DSI**, so a DSI panel is an internal
output and binds any internal-flagged provider device under the same
flag-only rule. Consequences, binding for this design:

1. EDID **cannot disambiguate KWin's internal path**. Any internal-flagged
   device matches any internal output regardless of EDID.
2. An ambiguous internal provider device therefore does not reliably fail
   unavailable: with N ≥ 2 internal outputs or M ≥ 2 internal devices it
   binds an arbitrary device to an arbitrary output. "Register and let
   KWin match" is fail-open. v2's ambiguous-rig fallback is withdrawn.
3. The protocol's `set_edid` is a base64 string of exactly the first 128
   EDID bytes [U].

Registration gate [P] — the provider registers exactly one internal
brightness device if and only if **all** hold:

- **G1 — unique backlight.** Exactly one eligible backlight device after
  the kernel's documented firmware > platform > raw preference;
  `eligible` = readable `type` ∈ {firmware, platform, raw} plus readable
  `max_brightness`. Multiple devices sharing the winning type →
  `unavailable(ambiguous-backlight)`. Zero eligible →
  `unavailable(no-backlight)`.
- **G2 — unique internal output.** Exactly one connected internal-class
  connector, read read-only from `/sys/class/drm/*/status` (value
  `connected`), the internal class being KWin 6.6.5's exact
  `isInternal()` set — kernel naming classes `eDP*`/`LVDS*`/`DSI*`
  [U `1787897128`; naming-class reliance [I], row R19 pins it including
  the one-DSI and mixed-eDP/DSI counterexamples]. A connected DSI panel
  counts exactly like eDP/LVDS; omitting it is the v3-review P1-A
  fail-open and is repaired here. Zero →
  `unavailable(no-internal-connector)`; two or more →
  `unavailable(ambiguous-internal-topology)` (so one eDP panel plus one
  DSI panel registers nothing — the verdict's counterexample becomes a
  named row input, R15).
- **G3 — totality by construction.** With one internal device and one
  internal output, KWin's internal match is the same unique pair for any
  iteration order, so no association heuristic is needed or used.
  Order-independence is proven by rows R16–R17, not assumed.

EDID handling under the gate: when the panel connector's
`/sys/class/drm/<connector>/edid` is readable, the provider sends
`set_edid` as base64 of exactly the first 128 bytes — for identity
completeness only; it is never a matching input for internal devices
(consequence 1). An unreadable panel EDID does **not** block
registration under G1–G3. v2's wording that the provider supplies "an
EDID beginning" unspecified is replaced by this exact definition.

Gate re-evaluation: on registration, on resume, and on a bounded 2 s
cadence while registered [P; [I] until R18 passes]. Any gate regression
destroys the registered device (protocol destroy request) and publishes
the typed unavailable reason; recovery re-registers. External-output
hotplug never affects the gate (G2 counts internal-class connectors
only). Gate inputs are read strictly read-only; `set_uses_ddc_ci` is
never set true in v1 [U: client declaration]. QindaQt never patches KWin
[B].

### 8.3 Apply path

Carried verbatim from v2 §8.3: each compositor `requested_brightness`
applies via logind `Session.SetBrightness("backlight", <device name>,
<value>)`; session path from `Manager.GetSession("auto")` [U]; capability
check (seated, seat-active session, caller uid = session user, device on
seat), not polkit; main-thread dispatch; lineage-stamped results.

### 8.4 Error truth (P1-8)

Carried verbatim from v2 §8.4: the compositor apply is fire-and-forget
and reports `applied` regardless of hardware outcome; the only hardware
truth is the device's subsequent observed report [U]; typed per-device
degradation (logind error, sysfs disappearance, non-convergence);
D7's class-B `Applied`/`Unconfirmed` semantics routed to the D7
definition; the withdrawn expectation about the protocol `failure_reason`
event stays retracted.

### 8.5 External-change observation (new, P1-2)

Pinned contract [U `1787895220` lines 58-74]: when brightness changes due
to external factors, the client must either overwrite the change with the
last compositor request or commit a new `set_observed_brightness`. The
overwrite alternative is **rejected**: it would contradict KWin's
user-adjustment learning that v2 decision 4 and §6.1 rely on [U]. The
provider always commits `set_observed_brightness` for external change.

Production source policy [P] — event-driven where reliable, bounded
measured fallback otherwise:

- **Primary:** an inotify watch on the selected backlight's
  `actual_brightness` sysfs attribute, via the `observe()` subscription
  of `BrightnessDevicePort`. Kernfs attribute writes produce fsnotify
  events on modern kernels [I: kernel-version/driver variance is exactly
  why the fallback is mandatory, proven by R24].
- **Bounded fallback:** a 500 ms reconciliation poll on the same
  attribute, active whenever the watch could not be established, and a
  2 s parity re-read always while registered — plus a mandatory re-read
  on every resume (`PrepareForSleep(false)`) and after each apply's
  convergence window. Poll bounds: 500 ms fallback cadence; both cadences
  are constants named in the port contract and asserted by rows R24–R25.
- **Debounce:** a 50 ms coalescing window; at most one commit per window
  carrying the latest read value.
- **Echo suppression (self-change vs external):** after an apply
  dispatch, observed values equal to the applied value inside a 250 ms
  convergence window are convergence, not external change (no commit);
  a differing value in the window is external and commits normally.
  Outside the window every value change commits. Values are compared on
  the wire integers, never percentages.
- **External-change handling:** read `actual_brightness` read-only →
  update the Power1 snapshot's last-observed value → commit
  `set_observed_brightness(<value>)`. No snapshot revision on identical
  values [B no-op rule]. Last writer wins by commit order; changes racing
  an in-flight apply are never merged.
- **Ownership and lifetime:** the subscription is owned by the provider
  collaborator, one per registered device, created at registration,
  closed at teardown/disappearance/epoch turnover; no watch exists before
  registration.
- **Disappearance mid-watch:** `IN_DELETE_SELF` or a read failure marks
  the device `unavailable`, destroys the protocol device, and closes the
  subscription (§8.6).

### 8.6 Teardown and replacement

Carried from v2 §8.5 with the §9 interplay: Wayland connection loss —
drop the device and exit (KWin removes devices on client disconnect [U]);
session-bus loss — exit; restart — re-register fresh under the §8.2 gate,
no retained handles, new epoch; reconnection follows §9's
publication-generation sequence, never a provider-side autonomous
reconnect. DDC/CI stays honest-unavailable in v1 (PB-6 reserved; never
inferred from the client-declared `uses_ddc_ci` flag [U]).

## 9. Session-lane publication and activation contract (new, P1-3) [P]

**Owner: the `qindaqt-session` session supervisor.** It is the
session-lane process that KWin directly launches to control compositor
lifetime and that starts exactly the notification host and shell [R
`docs/wiki/architecture/compositor-session.md:63-74` (integration tree);
`src/session_supervisor/src/session_process_supervisor.cpp:70-97`]. It is
therefore provably alive after the child compositor exists and already
owns session lifetime; Power1 is non-essential and must never become a
supervisor child. The supervisor is not a Wayland client itself: it
publishes a socket *name*; only Power1 connects.

**Sequence (one environment generation per publication):**

- **S1 — prove the socket exists.** After the child compositor is up, the
  supervisor stats the child socket (read-only; exists, is a socket). The
  child socket name is the sanitized per-session name the launcher
  already validates [R compositor-session page: sanitized child socket
  name].
- **S2 — publish atomically to both environments, by each endpoint's
  real API (v4: exact names and signatures, P1-B).** Exactly two calls,
  both carrying `WAYLAND_DISPLAY=<child socket name>` **and**
  `QINDAQT_SESSION_WAYLAND_SOCKET=<same value>`:
  (a) to the systemd user manager —
  `org.freedesktop.systemd1.Manager.SetEnvironment(as)` with both
  assignments in the one string array [U systemd-257: systemd exposes
  `SetEnvironment(as)` (and `UnsetAndSetEnvironment(as,as)`) and has no
  `UpdateActivationEnvironment` method; Dorian `1787897128`];
  (b) to the D-Bus daemon —
  `org.freedesktop.DBus.UpdateActivationEnvironment(a{ss})` with the
  same two pairs [U D-Bus specification]. A helper executable may issue
  the two calls; the service-manager call remains `SetEnvironment`. The
  supervisor awaits both method replies before proceeding; an error
  reply or reply timeout on either call fails the publication half of
  the generation — never masked, retried only within the S6 count, and
  blocking S4 until both replies have landed. The marker key exists so
  lineage is provable: an inherited stale `WAYLAND_DISPLAY` alone cannot
  satisfy the gate.
- **S3 — prove the value.** Both replies received (S2) + socket stat from
  S1. Full inheritance proof is only obtainable from the activated
  process itself, so the gate's acceptance is Power1's own readiness
  report (S5) and the supervisor treats registration or typed failure as
  the generation's outcome.
- **S4 — unconditional activation, arbitration-gated, one uniform count
  per generation (v4: count unified, P2-A).** The supervisor activates
  `org.qindaqt.Power1` via `StartServiceByName` (D-Bus →
  systemd user unit; no consumers required — no applet, no shell page,
  nothing else need exist). The per-generation activation budget is
  exactly **one initial attempt plus at most two retries** (three
  attempts total, on the S6 backoff) — stated identically in S4, S6, and
  R33; never "exactly one" and never an open-ended series. Only a
  supervisor that is the current S8 arbitration winner may activate.
- **S5 — Power1's gate (fail-closed before any connect).** On startup,
  before constructing any Wayland connection, Power1 requires:
  `WAYLAND_DISPLAY` present **and** equal to
  `QINDAQT_SESSION_WAYLAND_SOCKET` **and** that name resolving to an
  existing socket under `XDG_RUNTIME_DIR`. Any miss → exit nonzero
  before any connect attempt: no default `wayland-0` fallback, no
  inherited-name reuse, never a host or parent-compositor attach. On
  pass: connect, require the `kde_external_brightness_v1` global to be
  present (its absence is a typed failure and exit), and publish
  `wayland_binding` (socket name, protocol version, binding epoch) in
  its snapshot.
- **S6 — bounded retry, supervisor-owned, one count (v4: count unified
  with S4, P2-A).** Per generation: the S4 budget — one initial
  activation attempt plus at most two retries (three attempts total),
  1 s/2 s/4 s backoff; a failed S2 publication (error reply or timeout on
  either manager call) consumes the same budget before activation is
  attempted. Exhaustion records a typed unavailable outcome in the
  session lane's own status and stops; no infinite retry; and a
  takeover never resets a consumed count mid-generation — a fresh budget
  exists only in a new generation (S8). A new generation begins on
  child-socket recreation (compositor restart), on observed provider
  exit while this supervisor is the current arbitration winner (S8), or
  on an arbitration flip that makes this supervisor the winner.
- **S7 — early activation.** Any activation before S2 (stray consumer,
  manual start) fails S5 and exits without connecting anywhere. The
  failed early start consumes no retry generation.
- **S8 — deterministic single-session arbitration; winner-only takeover
  (v4: replaces v3's unconditional wrong-lineage takeover, P1-C).**
  Before S2 the supervisor evaluates the arbitration selector against
  shared read-only logind session truth (`Manager.ListSessions` [U]):
  among the same user's graphical sessions the single winner is the
  session that is active on its seat; if zero or several qualify, the
  deterministically lowest session ID among active-or-online graphical
  sessions [P; convergence and the no-loop property are proven at R34].
  A supervisor whose session is **not** the winner publishes the typed
  outcome `unavailable(multi-session-loser)` in its own session-lane
  status and does nothing else: **no environment publication, no
  activation, no retry, and never a `StopUnit` of the winner**. It
  re-evaluates on logind session-change signals and on its own
  generation triggers, and may run a generation only after a flip makes
  it the winner. The winner runs S2–S4; before S4 it reads the running
  instance's `wayland_binding` (if the name is owned): equal to this
  session's socket → idempotent, no re-activation; different → the
  winner alone stops that instance (`StopUnit`), then
  runs S2–S4, and the takeover is logged. The displaced lineage's
  supervisor re-evaluates arbitration, finds itself the loser, and
  publishes typed unavailable — the displaced lineage never re-activates,
  so a takeover cannot echo. Because only the deterministic winner may
  stop or start, and losers never stop the winner, v3's fight trace
  (each supervisor observing the other's takeover and re-taking, each
  takeover resetting the retry bound) is unreachable by construction;
  R34 proves the simultaneous-start and flip cases assert zero
  stop/restart cycles throughout.
- **S9 — multiple same-user graphical sessions.** Unsupported in v1 and
  declared so, but deterministic under S8 arbitration (v4: replaces
  v3's last-writer-wins, P1-C): the per-user activation environment and
  the single `org.qindaqt.Power1` name are shared, and S8 selects
  exactly one owning session. Non-owning supervisors publish
  `unavailable(multi-session-loser)` and never touch the shared
  environment or name; the owning supervisor's `wayland_binding`
  therefore always names the one bound compositor — never silently
  ambiguous, and reached without stop/restart churn (R34). A deliberate
  hand-off between same-user sessions is an arbitration flip followed by
  the new winner's single S8 takeover, not a fight.
- **S10 — socket loss and restart.** Compositor death → provider Wayland
  loss → Power1 exits (carried decision 9); supervisor re-publishes on
  socket recreation → new generation.

**Session-manager routing ask:** the S1–S10 additions to
`qindaqt-session` are a session-lane change requested through the
manager; Power lane consumes the contract, it does not implement it.

## 10. Failure policy (consolidated, v3)

Typed, fail-closed, epoch-scoped; no silent degradation anywhere.

| Failure | Detection | Action | Truth published |
| --- | --- | --- | --- |
| Identity gate fails (G1/G2) | discovery/inventory read | register nothing; gate re-evaluation continues | `unavailable(<identity reason>)` in snapshot |
| Identity gate regresses while registered | 2 s cadence / resume | destroy protocol device; unsubscribe | typed unavailable reason |
| Apply fails (logind error) | typed D-Bus error | no retry (P1-8 semantics; convergence check decides) | device `degraded(reason)`; compositor apply still reads `applied` |
| Non-convergence after apply | convergence window read | none (hardware truth wins) | `degraded(reason)` |
| External change | observe subscription / fallback poll | commit `set_observed_brightness`; snapshot update | last-observed updated |
| Sysfs disappearance | watch terminal event / read failure | destroy device; close subscription | `unavailable` |
| Wayland loss | connection drop | exit | service absent = honest unavailable |
| Session-bus loss | bus disconnect | exit | — |
| Upstream owner replacement | `NameOwnerChanged` / epoch rule (§5.1) | new epoch; invalidate handles | results uncertain, never replayed |
| Activation before publication | S5 gate | exit nonzero, zero connect attempts | service absent |
| Gate value stale/wrong socket | S5 equality + existence check | exit, no fallback names | service absent; supervisor records typed outcome |
| Retry exhaustion | supervisor counter (initial + ≤2 retries, P2-A) | stop retrying until a new generation | session-lane typed unavailable outcome |
| Publication call fails (either manager, P1-B) | error reply or reply timeout on `SetEnvironment(as)` / `UpdateActivationEnvironment(a{ss})` | consume the same S6 budget; no S4 attempt until both replies land; typed outcome at exhaustion | session-lane typed unavailable outcome |
| Supervisor loses arbitration (S8, P1-C) | logind session-truth re-evaluation | publish typed unavailable; zero publications, activations, retries, `StopUnit` calls | `unavailable(multi-session-loser)` |
| Arbitration flip to this supervisor (S8) | session-change signal / generation trigger | new generation; winner's single S8 takeover if a foreign binding runs | logged takeover; `wayland_binding` names this session |
| Wrong-lineage running instance | S8 binding read **by the winner only** | winner's `StopUnit` + S2–S4; logged; losers never stop the winner | `wayland_binding` always names the bound compositor |
| Partial inhibitor acquisition | transaction result | release all acquired; zero key actions | controller degraded status; logind defaults honestly in force |
| Inhibitor loss while held | event + `ListInhibitors` verification | unregister all three key actions; re-acquire | same as above during the gap |
| Shutdown during confirmation | `PrepareForSleep(true)` / delay expiry | drop pending confirmation, never dispatch | — |
| PPD absent / legacy name | carried v2 degradation rules | reduced capability truth | documented degraded provider |
| Malformed/oversized upstream data | total decoding (carried) | reject atomically; no partial replacement | bounded diagnostic |

## 11. Verification plan (v3; ordered rows)

Harness rules carried (P3-21): every fake-daemon row runs on a private
bus by overriding `DBUS_SYSTEM_BUS_ADDRESS`; the host system bus is never
touched; the host logind never sees a test inhibitor. Provider and
activation rows additionally use a private `XDG_RUNTIME_DIR` with fake
sockets and fake systemd-user-manager / D-Bus-daemon endpoints that
assert the exact S2 call names and signatures (R36); the parent/host
compositor socket, when modelled, is a decoy
inside that private dir. Nested rows prove protocol modelling only,
never transport — the virtual backend cannot represent internal/EDID
devices (P2-11) [U]; model-level convergence is fake-port evidence by
design.

Deterministic tiers, ordered [Q-det]:

- **R1** `qindaqt.power-protocol-*` units: bounds, round-trips, stable
  encoding, hostile-string sanitization (NUL, control, oversize, mixed
  lineage), enum and NaN/infinity rejection, no partial replacement;
  inhibitor sanitize proves `uid`/`pid` absence; backlight status
  vocabulary round-trips **including the four new identity reason codes**
  and the `wayland_binding` field (P3-6 note: these run from
  `tests/services/power_protocol/`).
- **R2** `qindaqt.power-aggregation-*` units: single/dual battery, UPS +
  battery, absent composite, coarse unknown-percentage levels, rate sign
  truth, warning mapping, time-estimate passthrough (from
  `tests/services/power_protocol/`).
- **R3** `qindaqt.brightness-model-*` units: fixture-keyed dedup,
  mirror-follows-source, capability loss removes only that entry,
  percentage via per-device min/max, keyboard rows, ambiguous-duplicate
  = no persistence context; no curve tests exist (from
  `tests/services/brightness_model/`).
- **R4** `qindaqt.idle-*` units: signal → `SetIdleHint` mapping, reset on
  activity, degraded session-path behavior.
- **R5–R6** `qindaqt.power-service-*` / `qindaqt.power-client-*`
  private-bus rows: carried in full from v2 §9 (fake UPower incl.
  singleton dedupe, fake PPD incl. holds/`ProfileReleased`/legacy
  degradation, fake logind incl. `PrepareForSleep` sequences,
  sanitization, §7.3 lifecycle, logind-restart [I] row,
  `NameOwnerChanged` epoch turnover, activation/replacement with epoch
  turnover, mid-operation uncertainty, absent-daemon degradation,
  atomic malformed rejection, bus-loss exit; client debounce, timeout,
  stale-reply, uncertain-once, stop-cancel). **No Wayland anywhere in
  R5–R6** (P2-14).
- **R7** partial inhibitor acquisition: fake logind grants two of three
  `handle-*` locks → zero KGlobalAccel actions registered, all acquired
  FDs released, degraded status truth.
- **R8** one-lock loss while held: fake logind closes one lock → all
  three key actions unregistered before the next key dispatch;
  re-acquisition success re-registers all three.
- **R9** logind replacement while held: epoch turnover → same as R8.
- **R10** shutdown during confirmation: `PrepareForSleep(true)` with a
  pending confirmation → dropped, never dispatched; delay-expiry variant.
- **R11** verification cadence: lock set re-verified via
  `ListInhibitors` on turnover and on the bounded cadence while actions
  are registered.
- **R12** one-to-one registration: fake discovery with exactly one
  eligible backlight and one connected `eDP*` connector → registers one
  internal device (`set_internal`, `set_edid` as base64 of exactly the
  first 128 bytes, `set_max_brightness`, `set_observed_brightness`,
  `commit`); the one-DSI variant (the single connected connector is
  `DSI*`) registers identically (P1-A counterexample basis).
- **R13** zero eligible backlights → `unavailable(no-backlight)`, no
  registration, no protocol device.
- **R14** multiple eligible backlights sharing the winning type
  (firmware-beats-platform variants included) →
  `unavailable(ambiguous-backlight)`.
- **R15** zero or two-plus connected internal-class connectors →
  `unavailable(no-internal-connector)` /
  `unavailable(ambiguous-internal-topology)` (external outputs present
  or absent must not change the outcome); the two-plus set must include
  the verdict's mixed counterexample — one connected `eDP*` panel plus
  one connected `DSI*` panel with one eligible backlight → ambiguous,
  no registration, no protocol device (P1-A).
- **R16** reordered enumeration: backlight and connector inventory
  shuffled → identical unique-pair registration (G3 order-independence);
  shuffles include the mixed eDP+DSI two-connector inventory, which
  stays ambiguous under every order because KWin consumes candidates in
  iteration order [U] (P1-A).
- **R17** hotplug regression: second internal-class connector appears →
  registered device destroyed, typed unavailable; external-output
  hotplug alone → no change; recovery → re-registration.
- **R18** gate cadence and resume re-evaluation with the bounded cadence
  constant asserted.
- **R19** internal-class naming pinned to KWin's exact internal set:
  `eDP*`/`LVDS*`/`DSI*` count toward G2 — a single connected DSI panel
  with one eligible backlight registers (cross-check of R12's variant);
  non-internal-class connector names (HDMI*/DP*/VGA*) never count toward
  G2 even when connected with a backlight present [I pinned by this row;
  classification injected per §4.1 from the pinned adapter set, P1-A].
- **R20** deterministic external change: fake port emits observed change
  → snapshot last-observed updated **and** `set_observed_brightness`
  committed — never an overwrite of the compositor request.
- **R21** echo suppression: apply then observed == applied inside the
  convergence window → no commit; differing value in-window → commit.
- **R22** debounce: burst of changes inside 50 ms → one commit with the
  latest value.
- **R23** change racing in-flight apply: last writer wins by commit
  order; no merge.
- **R24** fallback poll: inotify unavailable → 500 ms bounded poll
  detects the change and commits (cadence asserted).
- **R25** parity re-read and resume re-read: missed-event recovery within
  the 2 s parity bound; mandatory post-resume read.
- **R26** disappearance mid-watch: terminal watch event → `unavailable`,
  destroy, subscription closed.
- **R27** early activation: activated before publication → S5 exit,
  zero connect attempts (asserted at the fake-socket layer).
- **R28** no-consumer activation: supervisor publishes + activates with
  zero applets/clients → provider registers; no consumer needed.
- **R29** stale environment: `WAYLAND_DISPLAY` present but marker absent
  or unequal; and marker pointing at a nonexistent socket → S5 fail,
  bounded retry, **no connect attempt to any fallback name** (decoy
  `wayland-0` in the private dir proves it).
- **R30** zero host connection: parent/host socket present as decoy →
  every `connect()` target is the child path; none else.
- **R31** socket loss mid-session and restart: provider exit; new
  generation re-publishes and re-activates; fresh registration, new
  epoch.
- **R32** winner-only wrong-lineage takeover (S8, P1-C): a supervisor
  that is the arbitration winner stops and replaces a foreign-bound
  instance; the idempotent same-binding path does not restart; the
  displaced instance's supervisor re-evaluates, publishes typed
  unavailable, and issues **zero** `StopUnit` calls and **zero**
  activation attempts.
- **R33** retry exhaustion (P2-A): the initial attempt plus two retries
  fail → bounded stop, typed outcome; no further attempts until a new
  generation; a mid-generation takeover never resets the consumed count
  (fresh budget only in a new generation).
- **R34** simultaneous two-supervisor arbitration (S8/S9, P1-C; replaces
  v3's two-generation last-writer row): two same-user supervisors start
  concurrently and race → exactly the deterministic winner publishes and
  activates; the loser publishes `unavailable(multi-session-loser)` with
  zero activation attempts and zero `StopUnit` calls; then an
  arbitration flip → the new winner performs exactly one takeover and
  the displaced supervisor publishes typed unavailable and never
  restarts; the trace asserts zero stop/restart cycles throughout —
  convergence, not last-writer.
- **R35** fake-compositor convergence rows (protocol modelling only,
  labelled as such, P2-11): the fake compositor mirrors the **exact**
  KWin semantics from §8.2 — internal flag equality decides internal
  matches immediately; internal outputs are exactly LVDS/eDP/DSI per the
  pinned adapter set, so the fake binds a device across mixed eDP/DSI
  rigs and the gate's refusal is proven upstream of it (P1-A); EDID is
  consulted only for non-internal outputs;
  candidate consumption order is modelled to prove R16–R17 outcomes —
  replacing v2's "internal flag plus EDID beginning" fake rows.
- **R36** fake-manager signature and error rows (S2, P1-B): the fake
  systemd user manager accepts exactly
  `org.freedesktop.systemd1.Manager.SetEnvironment(as)` carrying both
  keys, and the fake D-Bus daemon accepts exactly
  `org.freedesktop.DBus.UpdateActivationEnvironment(a{ss})` carrying the
  same two pairs; a cross-wired call (either name on the wrong endpoint)
  or a wrong signature is a harness failure; an error reply or reply
  timeout from either publication call → no S4 activation attempt in
  that pass, the S6 budget is consumed on publication retries, and
  exhaustion publishes the typed outcome with zero activation attempts
  ever issued in that generation.

Physical hardware matrix [Q-hw] (release lane, separately reported),
carried with two corrections: internal-panel writes across
`firmware`/`platform`/`raw` types; refused-write/degraded truth; external
DDC monitors honest-unavailable in v1; suspend/resume across teardown/
re-registration; keyboard-backlight hotkey; profiles with real drivers
and with the daemon absent; shell-as-caller polkit subject proof on a
real login session; lid close/open against logind defaults; idle-hint
correctness against real input. **New:** a physical
hotkey/firmware-originated brightness change row proving the §8.5
external-change path end-to-end (`set_observed_brightness` observable at
the compositor) [U-required row for P1-2]. Resource wording corrected per
ADR-0015: PSS, private dirty, and wakeup measurement before any ceiling
is recorded (no invented budgets [B]); the **1,024 MiB initial aggregate
idle PSS ceiling** — a ceiling, not a goal — is the frame of reference,
superseding every 500 MiB reference; tighter budgets only via a later
superseding decision [R ADR-0015].

## 12. Vertical slice order (v3)

| Order | Slice | Owner lane | Exact paths (brace-free) | Depends on | Gate to start | Evidence |
| --- | --- | --- | --- | --- | --- | --- |
| PB-0 | `power_protocol` values; pure battery/profile aggregation; pure `brightness_model` on the fixture boundary | Power | `src/services/power_protocol/**`, `src/services/brightness_model/**`, `tests/services/power_protocol/**`, `tests/services/brightness_model/**` — as three independently reviewable commits: (1) protocol values + protocol-root tests, (2) aggregation + protocol-root tests, (3) model + model-root tests | nothing | none — unique paths, start now | R1–R3 [Q-det] |
| PB-1 | `power_service` core: UpowerState, PowerProfiles, LogindState, KbdBacklight collaborators; `power_client`; activation + hardened unit + interface XML | Power | `src/services/power_service/**`, `src/services/power_client/**`, `src/services/power_service/data/**`, `tests/services/power_service/**`, `tests/services/power_client/**` | PB-0 | manager routing of this revision | R4–R6 [Q-det]; packaging rows (staged install contains executable, activation file, interface XML, hardened unit); owner/PID/epoch replacement with zero surviving state [B Audio precedent] |
| PB-2 | `power_backlight_provider` (§8: identity gate, apply, observation, error truth, teardown); `power_idle`; Wayland-loss exit; `brightness-routing.md` provider section | Power | `src/services/power_backlight_provider/**`, `src/services/power_idle/**`, `tests/services/power_backlight_provider/**`, `tests/services/power_idle/**`, docs path per route-registry owner | PB-1 | manager routing of the §9 session-lane contract (S1–S10, incl. v4 arbitration) to the session-lane owner (implementation of S1–S10 may land in parallel or first; provider rows need only the fake-port harness) | R12–R36 [Q-det]; internal-panel and DDC rows [Q-hw] in the release lane |
| PB-3 | Shell in-session session-action controller: seven-value `Can*` mapping, one-call-per-intent, §7.4 inhibitor transaction, KGlobalAccel actions | Shell | shell-lane paths per shell owner; consumes public clients only | Controls/overlay foundations; Power1 snapshot optional (degrades to own `ListInhibitors`) | after the shell owner exists (manager assignment) | R7–R11 [Q-det]; polkit subject proof [Q-hw] |
| PB-4 | D7 closed class-B policy values and typed method (carried) | Display | display-lane paths (D7) | D2 accepted | display lane schedule | display lane evidence; resolves the class-B provisional condition jointly with PB-2's provider rows [B] |
| PB-5 | Bind `brightness_model` to `display_client`; brightness shortcuts; Brightness/Power pages (carried) | Power/Shell/Native app | binding code + page paths per route-registry owner | PB-2, PB-4, AppShell routes | PB-4 contract accepted | binding + page rows; `brightness-routing.md` binding section |
| PB-6 | Reserved (carried): Settings power keys, user idle-action dispatch through the shell controller, lid override, charge thresholds, DDC/CI transport decision | mixed | ADR first | their ADRs | later | per future ADRs |

Ordering constraints carried: no Power path includes a display protocol
header before D7's contract is accepted (only PB-5 touches
`display_client`); PB-2 remains the hardest slice; PB-1 stays
Wayland-free. v3 change: PB-2's gate is no longer an open
activation-environment decision — §9 decides the owner and sequence;
what remains for the manager is routing the session-lane contract to the
`qindaqt-session` owner.

## 13. ADR topics (carried; numbers reserved by the manager)

1. Power1 split-upstream-authority service (carried verbatim, incl. hold
   lifecycle and legacy PPD degradation).
2. Session power actions are shell-invoked (carried; §13 adds the §7.4
   all-or-nothing/loss contract as part of the amended platform-plan
   section B placement).
3. Brightness routing without a Brightness1 process — extended: the
    provider as hardware truth; **fail-closed internal identity under
    exact KWin matching semantics, internal class = the pinned
    LVDS/eDP/DSI adapter set (P1-1; v4 P1-A)**; **external-change
    observation with commit-not-overwrite and its production-source
    policy (P1-2)**; **the session-lane publication/activation contract
    with exact manager call signatures (`SetEnvironment(as)` /
    `UpdateActivationEnvironment(a{ss})`), the unified
    one-initial-plus-two-retries budget, and deterministic
    single-session arbitration (P1-3; v4 P1-B/P1-C/P2-A)**;
    DDC honest-unavailable with reserved transport; compositor sole
    adaptive authority; error truth via provider + D7 confirmation.
4. Reserved (carried): in-session shell/compositor lid-policy slice with
   the block lid inhibitor and the no-KWin-patch rule.

## 14. Open items, risks, and labelled inference (v3)

1. `LockOnResume` default unproven [I]; KWin-internal, not a QindaQt
   gate (carried).
2. Inotify reliability on backlight sysfs attributes varies by
   kernel/driver [I]; the bounded 500 ms fallback and 2 s parity re-read
   exist because of this and are proven by R24–R25.
3. The 2 s gate/verification cadences are proposed constants [P],
   labelled [I] until R18/R11 pass; they are port-contract constants,
   not heuristics.
4. `eDP*`/`LVDS*`/`DSI*` internal-class naming reliance [I], pinned by
    R19; the classification is injected per §4.1 from KWin 6.6.5's exact
    `DrmConnector::isInternal()` set [U `1787897128`], whose DSI member
    is the v3 review's P1-A repair.
5. The `QINDAQT_SESSION_WAYLAND_SOCKET` marker and the exact publication
   pair — `org.freedesktop.systemd1.Manager.SetEnvironment(as)` plus
   `org.freedesktop.DBus.UpdateActivationEnvironment(a{ss})` [U] with
   the [P] supervisor-side call wrapper — are falsifiable at R27–R30 and
   R36 without live-session claims.
6. Logind-restart survival of held inhibitors [I] with its dedicated
   fake-logind row (carried).
7. Pinned protocol XMLs must be vendored and checksummed by implementing
   slices (carried).
8. Charge thresholds excluded from v1 (carried). Manual brightness keys
   at the lock screen stay a locker/compositor question (carried).
9. KWin 6.6.5 internal DDC/CI future affects only PB-6 (carried).
10. v2 open item closed: the activation-environment owner is decided
    (§9); the old "mechanism remains [I]" note is superseded by item 5.
11. The S8 arbitration selector — active-session preference, then the
    deterministic lowest session ID over shared read-only `ListSessions`
    truth [U] — is [P]; its convergence and no-loop properties are
    proven at R34, and a same-user graphical session is the only scope
    in which it has any effect (single-session hosts never evaluate a
    contested selector).

## 15. Peer questions and exact rereview request

To **Dorian Vale** — exact rereview request for this revision only (v3
`1787896208` is withdrawn; this post is the sole candidate), covering
exactly the four findings of your v3 verdict `1787897128`; the four
dispositions you marked PASS in the same verdict (P1-2 external-change
observation, P2-4 inhibitor atomicity/loss, P3-5 the 1,024 MiB ceiling,
P3-6 explicit PB paths) carry forward untouched and need not be
reopened:

1. *P1-A — DSI in the fail-closed identity gate.* Please verify §8.2's
   pinned set (KWin v6.6.5 `DrmConnector::isInternal()` = LVDS ∨ eDP ∨
   DSI per your `drm_connector.cpp:202-205` citation), §2 decision 11,
   §4.1's typed injected inventory classification, and the named
   counterexamples in R12 (one-DSI variant), R15 (mixed eDP+DSI →
   ambiguous), R16 (mixed shuffles stay ambiguous), R19 (DSI counts;
   HDMI*/DP*/VGA* never), and R35 (fake compositor's internal set).
2. *P1-B — exact activation APIs.* Please verify §9 S2's two calls —
   `org.freedesktop.systemd1.Manager.SetEnvironment(as)` [U systemd-257]
   and `org.freedesktop.DBus.UpdateActivationEnvironment(a{ss})` [U
   D-Bus specification] — with both replies awaited before S4, the §10
   publication-failure row, and the fake-manager signature/error row
   R36.
3. *P1-C — deterministic single-session arbitration.* Please verify §9
   S8/S9 (selector over shared read-only logind truth; losers publish
   `unavailable(multi-session-loser)` and never stop the winner, never
   publish, never activate, never retry; winner-only S8 takeover that
   cannot echo), §2 decision 13, the §10 arbitration rows, and rows
   R32/R34 — R34 being the simultaneous-two-supervisor row you required,
   proving convergence and zero stop/restart cycles through the flip
   phase.
4. *P2-A — one unified activation count.* Please verify §9 S4/S6, the
   §10 retry row, and R33: one initial attempt plus at most two retries
   (three attempts total, 1 s/2 s/4 s), stated identically everywhere,
   with fresh budgets only in new generations.

To the **manager**: routing items — (a) route the §9 session-lane
contract (S1–S10, including v4 arbitration) to the `qindaqt-session`
owner as PB-2's gate; (b) accept decisions 1–15 as amended in v4
(decisions 11 and 13) and the §9 arbitration/activation contract;
(c) place PB-3 with
the shell lane when a shell owner exists; (d) route P1-8's
confirmation semantics to the D7 definition (carried); (e) allocate ADR
numbers for §13 topics 1–3 (4 reserved). No Power implementation slice
is assignable until one accepted revision is integrated; PB-0 and the
Wayland-free portions of PB-1 remain technically separable candidate
work per your verdict note.

To **Elara Finch**: your 22 dispositions carry forward unchanged (§1.1
map); the v4 deltas inside previously accepted sections are exactly
Dorian's four repairs, listed there.

To **Rhea Calder and Kellan Ward**: carried asks stand (D7 carries
per-output `auto_brightness`, capability/error truth,
`Applied`-only-after-confirmation); no new display-lane ask in v3.

To the **session-lane owner** (via manager routing): §9 is the ask; the
supervisor gains S1–S10 and no Wayland client behavior; Power1 stays
non-essential and outside the supervisor's child set.

To the **shell lane owner** (via manager routing): PB-3 now includes
the §7.4 transaction semantics and its rows.

To **Ada Ruiz** (Settings lane): carried unchanged (reserved schema
slice = keyboard-backlight idle timeout only).

To the **native-app lane**: carried unchanged (degraded vocabulary;
`wayland_binding` joins the snapshot for truthful service context).

## 16. Non-claims

Carried from v2 §14: this is static architecture repair; it proves no
runtime, build, test, nested, or hardware behavior. Items marked [I]
(require the named evidence rows before reliance): `LockOnResume`
default; logind-restart inhibitor survival; the shell's session-scope
membership under a real display manager; inotify-on-sysfs reliability
and the cadence constants; `eDP*`/`LVDS*`/`DSI*` naming-class reliance;
the activation publication pair's supervisor-side wrapper (falsifiable
at R27–R30 and R36); the S8 arbitration selector (falsifiable at R34).
The verdicts' non-claims are inherited. No implementation slice is
assigned from v3 or v4 before acceptance; the product worktree remains
clean at exact base
`94e84077e33a279dcebee24511e7dbdf1b87e3e1`; nothing was compiled,
launched, or inspected on the host in any run; no upstream fetch
succeeded in the v3 or v4 runs, so no newly fetched citation exists.

Recap: v4 keeps v3's shape — one Power1 process, provider inside it,
compositor owns adaptation, shell owns authorization truth, model stays
pure behind the fixture boundary — and closes Dorian Vale's four v3
exact-review findings: the identity gate now uses KWin's real internal
set including DSI, with the one-DSI and mixed-eDP/DSI counterexamples
named as test inputs; the publication call is the real per-endpoint API
pair (`SetEnvironment(as)` / `UpdateActivationEnvironment(a{ss})`) with
awaited replies and fake-manager rows; same-user multi-session is
decided by deterministic single-session arbitration whose losers never
stop the winner and publish typed unavailable; and the activation
budget is uniform everywhere — one initial attempt plus at most two
retries. The six slices PB-0…PB-5 plus reserved PB-6 remain each
independently reviewable.

## 17. Self-identity

- Artifact path:
  `ops/team/messages/platform-power-brightness/1787897980-priya-nair-architecture-handoff-v4.md`
- Self-hash: 4dc346224fb9ae8a280f1253ce954eafedc5b608b379e84e727cc2c4d4acf224
- Hash definition: SHA-256 of this file's exact bytes with the value on
  the "Self-hash:" line replaced by 64 ASCII zeros (`0`).
- Verify:
  `sed '/^- Self-hash: /s/[0-9a-f]\{64\}/0000000000000000000000000000000000000000000000000000000000000000/' <file> | sha256sum`
  must equal the Self-hash value above (the address restricts the
  replacement to the Self-hash line; other hexadecimal content in the
  artifact, such as the superseded revision's SHA-256, is untouched).
