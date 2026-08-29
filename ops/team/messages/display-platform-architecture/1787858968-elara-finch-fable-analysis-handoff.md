# Claude Fable display/output architecture analysis

- Published by: manager
- Analyst runtime: `claude-fable-5`, maximum effort, read/search only
- Analyst base: `dc29c88911f0ed6d381211027f16f46bbf92a07c`
- Verified session: `393ebc49-9070-49e5-83c5-2c0000b37ad5`
- Result: success; product worktree remained detached and clean
- Provenance: verbatim analyst result follows; manager decisions are recorded separately.

All evidence I can obtain is in hand; nothing further is fetchable that would change a decision, so the remaining work is the deliverable itself. Below is the complete board-ready handoff.

---

# QindaQt display/output platform architecture — analysis handoff

- **Analyst role:** Display and Output Architecture Analyst (analysis and planning only; no file was created, edited, or committed; no build, test, or runtime command was run)
- **Product base inspected:** `dc29c88911f0ed6d381211027f16f46bbf92a07c` (clean detached worktree `/home/cabewse/work_SPaC3/container-wm-workers/fable-display-architecture`)
- **Board read (read-only):** `ops/team/messages/{platform-services,native-application-design,desktop-experience-coordination,display-platform-architecture,persistent-notification-quieting}/`, `ops/team/workers/*.md`
- **Upstream pins consulted:** KWin `v6.6.5`, libkscreen `v6.6.5`, kscreen `v6.6.5`, layer-shell-qt `v6.6.5`, plasma-wayland-protocols `v1.20.0` (the minimum KWin 6.6.5 requires) and `master`, systemd `main` man sources, Qt 6.11 documentation
- **Evidence legend used throughout:** **[R]** repository fact (exact path:lines) · **[U]** upstream primary-source fact (linked) · **[B]** board record · **[I]** inference (labelled; not evidence) · **[P]** proposed contract · **[Q-det]** deterministic qualification (unit / private D-Bus / nested KWin) · **[Q-hw]** physical-hardware qualification (release lane, separately reported)
- **Fetch limitations to record:** every official Qt Wayland *source* URL (code.qt.io tree/plain, Qt's GitHub mirror, rendered pages) returned 404 through the fetch tool; freedesktop GitLab raw files are behind an anti-bot gate (403). Qt-side derivation facts below therefore rest on official Qt *documentation* plus repository nested evidence and are labelled **[I]** where the source could not be read. Wayland protocol semantics that could not be read from freedesktop were cross-checked from KWin's own implementation **[U]** and, for prose only, from the `wayland.app` mirror (labelled "mirror").

---

## 1. Executive decision summary

### Recommended architecture

1. **KWin 6.6.5 stays the sole live *and* restore authority for output configuration.** This is stronger than the wiki currently states: upstream KWin persists every successfully applied configuration in `kwinoutputconfig.json`, restores per-output-set "setups" on hotplug, generates defaults (scale heuristics, left-to-right placement), handles lid-closed setups, mirroring, and priority order **[U]** (`Workspace::applyOutputConfiguration` → `storeConfig`; `OutputConfigurationStore::queryConfig/findSetup/generateLidClosedConfig/applyMirroring`). QindaQt must not duplicate, read, or write that store. Doing so is the "second writer" trap.
2. **A focused, D-Bus-activated user service `qindaqt-display-service` (`org.qindaqt.Display1`) is QindaQt's single writer to KWin's `kde_output_management_v2` protocol.** It owns: the display transaction state machine (stage → validate → preview-apply → observe → confirm | timeout-revert), the **confirmation timer**, a persistent revert journal, persistent output identity/aliases/registry (through Settings1), and the typed non-confirm "policy" property path that Color1 and the brightness model will consume. It is not a `Platform1` process: it has no audio/power/network/bluetooth/color/font/portal code and no shell or QML dependency.
3. **Display1 speaks the protocol directly through a private, generated Wayland client adapter** (`kde-output-device-v2` v20, `kde-output-management-v2` v19, `kde-output-order-v1` v1, `xdg-output` v3 — exactly the versions KWin 6.6.5 serves **[U]**), with the XMLs vendored and pinned like the KWin pin. libkscreen is the documented, rejected alternative (audit in §3 and §13); `kscreen-doctor` (part of libkscreen) is admitted only as an optional *test oracle*.
4. **The compositor plugin changes only additively:** an output generation counter and richer per-output fields on Compositor1 `Outputs` (protocol 1.1), a development-only virtual-hotplug seam built on the exported `KWin::OutputBackend::createVirtualOutput/removeVirtualOutput` API **[U]**, and a Hybrid slice that reflows a container as one unit after KWin's per-window output reassignment.
5. **The shell's geometry path is unchanged** (Qt screens + Compositor1 bijection). The shell gains a narrow `DisplayFacade` (alias resolution, confirmation/identify overlays on every enabled output, a KGlobalAccel "Revert display changes" action). It never depends on Display1 for panel geometry, so a stale or absent Display1 cannot produce partially published shell state.
6. **Settings1 owns QindaQt display *policy and registry* keys only** (aliases, labels, confirm timeout, primary preference, HDR policy, fractional-scaling preference); it never becomes a topology authority and no cross-key invariant is introduced, so the currently single-key Settings1 client **[B]** suffices.
7. **Lid/suspend v1 = upstream defaults, made transaction-safe:** KWin's lid handling and logind's lid defaults are left alone; Display1 only holds a short `sleep` *delay* inhibitor while a preview is live and cancels/reverts on `PrepareForSleep(true)`. User lid overrides are a later Power1+Display1 slice with an explicit open question (KWin's lid behaviour is not configurable without a downstream patch).

### Why this is the correct boundary

- It preserves every stated truth: Wayland-first KWin reuse without touching internals (zero downstream patches remain zero); compositor owns live state; platform policy owns heterogeneous topology *application* through the compositor's own public protocol; geometry stays desktop-logical with scale as metadata; `(panelId, outputId)` identity and atomic shell publication are untouched; Hybrid groups keep moving as one; UI consumers see only `display_client`/view models; persistence goes through Settings1; nested evidence is never reported as DRM/GPU/lid/suspend evidence.
- It is the smallest boundary that can *guarantee rollback after Settings Center loss* (service-owned timer + journal), which no in-app or in-plugin design can.
- It keeps the ABI-pinned plugin small (observation + test seam + Hybrid reflow) and keeps mutation authority where an out-of-process transaction can survive the UI.

### The five highest-risk decisions

| # | Decision | Risk | Mitigation in this plan |
| --- | --- | --- | --- |
| 1 | KWin store = restore authority; Display1 never touches `kwinoutputconfig.json` | KWin persists an *unconfirmed preview* immediately **[U]**; if nobody reverts, the next login restores it | Persistent journal in `$XDG_STATE_HOME`, revert-on-restart, shell activates Display1 at login; residual risk documented (§5) |
| 2 | Direct protocol adapter instead of libkscreen | Re-implementing ~500 lines of protocol plumbing; version drift | Pinned XMLs with SHA manifest matching KWin's required plasma-wayland-protocols 1.20.0 (mgmt 19/device 20 = KWin's served versions **[U]**); adapter gates every request by bound version; libkscreen retained as revisit option |
| 3 | Service-owned confirmation timer + rescue (overlay on every output + global shortcut + auto-revert) | Dialog unreachable, keyboard-only users, no shell | Timer authority is never the UI; three independent rescue paths; refuse previews while locked |
| 4 | Persistent identity mirrors KWin's matching precedence | QindaQt registry and KWin store disagreeing on "same monitor" for duplicate-EDID monitors | Same precedence by construction (EDID identifier → EDID hash → MST path → connector) **[U]**; ambiguity is surfaced, not guessed |
| 5 | Hybrid group reflow after KWin's per-window `desktopResized` pass, and the shell's exact-scale bijection at fractional scale | KWin moves members individually **[U]**; shell likely falls into permanent safe-visible fallback at 125 % **[I]** | Compositor slice D5 (group reflow + restore remap); shell slice D4 (drop DPR equality, keep name+geometry bijection) with nested 125/150 % rows |

### Routing-addendum decision map (board `1787854168`, items 1–10)

| Item | Resolved in |
| --- | --- |
| 1 integration choice & KWin compatibility | §3 (adapter), §13 (libkscreen rejected) |
| 2 persistent identity | §4 |
| 3 sole persistence authority / Plasma exclusion | §1, §5, §8, §12 (release question on KScreen OSD) |
| 4 candidate schema, lease, apply/confirm/rollback, crashes, hotplug during preview | §5, §9 |
| 5 atomic order to shell inventory/work areas/confirmation | §6 |
| 6 Hybrid migration/restore/rollback | §6, §7, slice D5 |
| 7 lid/suspend responsibility & inhibitor ordering | §8 |
| 8 capability ownership (brightness/DDC-CI/ICC/HDR/WCG/VRR/orientation/auto-brightness) | §3 (transaction classes), §12 platform question |
| 9 authorization boundary & residency | §3, §5 |
| 10 deterministic vs physical matrix | §10 |

---

## 2. Current truth audit

State vocabulary: **Implemented** (code + focused tests) · **Virtually qualified** (nested/virtual KWin evidence recorded) · **Nested-but-unqualified** (runs nested, no recorded proof for the property) · **Physically qualified** · **Declarative/virtual only** · **Planned** · **Missing/Open**.

| Area | What exists (evidence) | State | Gap relevant to this plan |
| --- | --- | --- | --- |
| Compositor output inventory | `Outputs` = `workspace()->outputOrder()` with `name`, `geometryF`, `scale`, `refreshRate` (mHz), transform kind, `isInternal` — `src/compositor/kwin/managedwindowregistry.cpp:377-391`; invalidation from `Workspace::outputsChanged` — `:72-73`, `src/compositor/kwin/kwincontrolendpoint.cpp:59-60`; XML `compositor/dbus/org.qindaqt.Compositor1.xml:11-13,54` | Implemented; virtually qualified for 1080p/WUXGA/1440p, 1080p@1.25, dual-1080p (`docs/wiki/development/testing-harness.md:356-362`) | No revision/epoch on `Outputs`; no uuid/priority/physical size; no disabled outputs |
| Shell visibility snapshot outputs | integral logical geometry from `LogicalOutput::geometry()` with the explicit "same as QScreen" comment — `src/compositor/kwin/kwinshellvisibilitypublisher.cpp:277-288`; per-output `geometryChanged`/`scaleChanged` refresh — `:191-204`; epoch/revision semantics — `docs/wiki/reference/compositor-control-v1.md:150-168` | Implemented; virtually qualified @100 % (`testing-harness.md:171-199`) | None for geometry; fractional scale never exercised in the production shell |
| Shell Qt output inventory | `QScreen::name()`, `geometry()`, `devicePixelRatio()` — `src/shell_surface/src/qt_output_inventory.cpp:74-97`; borrowed-pointer contract — `.../qt_output_inventory.h:37-40` | Implemented | `devicePixelRatio()` is an integer on Wayland (§7) **[U doc + I]** |
| Shell inventory bijection | exact count/id/geometry/**scale** equality — `src/shell_orchestration/src/output_inventory_matcher.cpp:47-49,59-73`; safe-visible fallback on mismatch — `src/shell/runtime/shellruntimeapplication.cpp:356-374`; role liveness recheck also requires `screen->devicePixelRatio() == outputScale` — `src/shell_surface/src/layer_shell_surface_backend.cpp:224-237` | Implemented; qualified @100 % only | **Suspected permanent safe-visible fallback on any fractional-scale output [I]** — verify first (slice D4) |
| Shell output-change handling | per-screen `geometryChanged/physicalDotsPerInchChanged/logicalDotsPerInchChanged/orientationChanged`, `screenAdded/Removed/primaryScreenChanged` → 0 ms single-shot debounce → one full reconcile — `shellruntimeapplication.cpp:60-67,327-337,439-454`; prior set retained on failure; compositor-dismissed roles recreated — `docs/wiki/shell/panel-surfaces.md:77-84` | Implemented; hotplug **nested-but-unqualified** (no hotplug row in the production-surface matrix) | Named profile selector that no longer exists **fails the solve** (`panel-surfaces.md:88-93`, `docs/wiki/shell/layout-profiles.md:71-73`) → undocking with a user profile naming `DP-1` breaks the whole panel set **[I from R]** |
| Notification surfaces | primary-screen only — `shellruntimeapplication.cpp:432-434`, `panel-surfaces.md:111-119` | Implemented; unqualified nested | Primary transfer moves them; no per-output overlay primitive yet |
| Launcher/topology | one common `--width/--height/--scale/--output-count` — `src/session/kwincommandbuilder.cpp:42-52,76-85`, `src/session/sessionoptions.h:15-31`; scenario path exported only; dev marker gating — `src/session/sessionenvironment.cpp:23-31` | Implemented | Heterogeneous topologies cannot be launched; must be produced *after* start through the protocol (§10) |
| Scenario catalog | 14 scenarios under `tests/scenarios/` (e.g. `laptop-external-hotplug.json` eDP-1@1.25+DP-1 1440p with enable/primary/disable events; `mixed-dpi-1080p-1440p.json` with `x:-1920`; `portrait-wuxga-1440p.json`; `dynamic-output-reconfigure.json`); validator `tools/qindaqt_dev/scenarios.py:15-37,159-184` | Declarative only (`testing-harness.md:51-57`) | No `mirror` event action exists (`scenarios.py:29-37`) although `dynamic-output-reconfigure.json:5` claims mirroring; transform vocabulary `flipped*` differs from Compositor1 `flip-x*` (`compositor-control-v1.md:68-70`); **`tools/qindaqt_dev/backends.py:101-110` notes claim "the compositor adapter replays per-output scenario state", contradicting `testing-harness.md:51-57`** — stale tooling prose |
| Hybrid output behaviour | queued per-container context on member `outputChanged` — `src/compositor/kwin/kwingroupcontextmanager.cpp:57-73,87-128`; outer-frame mapping between placement areas (relative-centre, keep-inside) — `src/compositor/kwin/hybridgroupcontext.cpp:91-118`, `src/compositor/kwin/kwinhybridreflow.cpp:205-230`; chrome DPR resample on `outputsChanged` — `src/compositor/kwin/kwinhybridsession.cpp:159-161,456-457`; restore preflight fails on unknown output, no remap — `src/compositor/kwin/kwinhybridplatform.cpp:110-124,181-183`, `docs/wiki/architecture/hybrid-constraints.md:85-89` | Implemented for *output change of a member*; mixed-DPI migration/hotplug explicitly unqualified (`docs/wiki/architecture/hybrid-topology.md:404-407`, `hybrid-constraints.md:200-203`) | No reaction to same-output geometry/scale change or output removal as a *group*; restore state references vanished output names |
| Settings | `displays.fractionalScaling`, `displays.primaryOutput`, `displays.configuration` (object), `displays.hdrPolicy` — `data/settings/schema-v1.json:84-108`; model supports multi-op staging — `src/settings/include/qindaqt/settings/settings_transaction.h:24-45`; no display consumer; Settings1 candidate `3de6bfa…` not yet integrated, wire 1–64 ops, high-level client single-key **[B ada-ruiz answers/handoffs]** | Data only | Keys are placeholders with no owner or semantics |
| Session supervision | exactly two essential children (host, shell) — `src/session_supervisor/include/qindaqt/session_supervisor/session_process_supervisor.h:26-29` | Implemented | Display1 must not become an essential child (§3) |
| KWin pin | 6.6.5 exact, zero patches — `compositor/upstream/kwin.json`; plugin links `KWin::kwin` + exact PlasmaActivities — `src/compositor/CMakeLists.txt:58-64,284-302` | Implemented | Any lid-policy override needs a first downstream patch (§8, §12) |
| Development seams | `InjectTestInput`, `ReinitializeCompositingForTest` gated pre-parse — `docs/wiki/reference/compositor-control-v1.md:170-223` | Implemented | No output/hotplug seam |
| Physical qualification | none; DRM path is "command construction" only — `docs/wiki/architecture/compositor-session.md:54-59,365-380` | Missing (release gate) | — |
| Board state | Display1 section is provisional and unassigned **[B 1787853847 §E, 1787854168, 1787854411]**; Displays route degraded in native IA **[B 1787853515 §5]**; `service_availability` values-only module accepted but to be created after two clients **[B 1787854166]** | Planned | This handoff supplies the missing contracts |
| Host observation (not repo) | `/usr/share/dbus-1/services/org.kde.kscreen.osdService.service` → `/usr/lib/kscreen_osd_service` and `org.kde.kscreen.service` → `/usr/lib/kf6/kscreen_backend_launcher` exist on this machine | Environment fact | KWin's generated-config OSD call would D-Bus-activate Plasma's OSD in a QindaQt session (§8, §12 release question) |

---

## 3. Module and process graph

### Processes

| Process | Owns | Must never own | Lifetime / restart |
| --- | --- | --- | --- |
| `qindaqt-wm` (KWin + `qindaqt_compositor` plugin) | live outputs, windows, Hybrid; KWin-internal persistence (`kwinoutputconfig.json` **[U]**) | any Display1 client, Settings1 client, policy | unchanged |
| `qindaqt-display-service` (**new**, `org.qindaqt.Display1`) | single QindaQt writer of `kde_output_configuration_v2`; transaction state machine + timer + journal; identity resolver; policy/registry through the public Settings1 client; `sleep` delay inhibitor during previews | shell surfaces, QML, KWin headers, colord/UPower/fontconfig, brightness policy, lid policy | D-Bus activated (`org.qindaqt.Display1.service`, optional `qindaqt-display.service` user unit `Type=dbus`); resident after first activation; exits on local session-bus disconnect (Settings1 precedent **[B 1787856640]**) and on Wayland connection loss; **not** a `qindaqt-session` essential child |
| `qindaqt-shell` | `DisplayFacade` (aliases, confirmation/identify overlays, revert shortcut) via `display_client` | raw D-Bus, Display1 implementation objects, timer authority | unchanged |
| `qindaqt-settings` (Settings Center) | Displays page + view model via `display_client` and Settings1 client | shell internals, compositor objects | unchanged (post-integration) |
| `qindaqt-settings-service` (Settings1) | `displays.*` key persistence/validation | display semantics | unchanged |
| later `qindaqt-color-service`, brightness model | consume `display_client` policy-class methods | protocol access | per Samira's plan **[B]** |

### Modules (new; all paths proposed **[P]**)

| Module | Responsibility | Allowed inward dependencies | Forbidden |
| --- | --- | --- | --- |
| `src/services/display_protocol` | versioned wire values (`DisplaySnapshot`, `OutputDescriptor`, `DisplayCandidate`, `TransactionSummary`), bounds, error codes, epoch/revision lineage, typed D-Bus (de)serialization | Qt Core/DBus | KWin, Wayland, QML, settings implementation |
| `src/services/display_identity` | pure `stableId` derivation, collision/ambiguity rules, alias map values, registry document schema | Qt Core | any transport, files |
| `src/services/display_topology` | pure candidate validation and geometry math (overlap/gap/normalization, logical rounding parity, transform, mirroring, primary), candidate diff, fingerprint | Qt Core; `display_protocol` values | shell types, KWin |
| `src/services/display_transaction` | pure deterministic state machine with injected clock and port interface; journal value; class A/B policy | `display_protocol`, `display_topology`, Qt Core | D-Bus, Wayland, timers with real clocks |
| `src/services/display_service` | orchestrator, D-Bus object, real timers, journal file (`QSaveFile`), Settings1 client use, logind delay-inhibitor adapter, lock-state observation; declares `CompositorOutputPort` interface | the four modules above, public Settings1 client, Qt DBus | KWin headers, QML, shell, colord/UPower |
| `src/services/display_compositor_adapter` | private generated Wayland client for the pinned XMLs implementing `CompositorOutputPort`; version gating; `done` batching → generations | Qt Core, Qt WaylandClient (generator `qt_generate_wayland_protocol_client_sources`), `display_service` port header | D-Bus, settings, shell |
| `src/services/display_client` | owner-bound asynchronous client: `ClientState{Unavailable, Authenticating, Ready, Degraded}`, mutation results `Applied | Rejected(code) | Conflict | Uncertain`, invalidation coalescing, stale-reply rejection, no replay (mirrors `src/shell_visibility_client/include/qindaqt/shell_visibility_client/compositor_visibility_client.h:16-45` and Settings1 client semantics **[B]**) | `display_protocol`, Qt Core/DBus | service implementation, QML |
| `src/sdk/service_availability` | (Samira-owned, created after two accepted clients **[B 1787854166]**); Display1 is a producer only | — | — |
| `src/shell_orchestration/…/output_alias_resolver` | pure alias → connector resolution over (profile, alias map, inventory) | `profiles`, `display_protocol` values, Qt Core | D-Bus |
| `src/shell_surface/…/layer_shell_overlay_surface` | backend-neutral per-output centred overlay role (keyboard interactivity on-demand, non-reserving) generalizing `layer_shell_notification_surface.h:18-29` | LayerShellQt in adapter only | policy |
| `src/shell/runtime/display*` | `DisplayFacade` (QML-narrow), `DisplayConfirmationController`, `DisplayRevertShortcut` (KGlobalAccel via existing registrar), overlay QML | `display_client`, `shell_surface`, `shell_orchestration`, design tokens | raw D-Bus |
| `src/compositor/kwin/kwinoutputinventory` | extracts `outputsJson` with `outputGeneration`, `uuid`, `priority`, `physicalSizeMm`, `manufacturer`, `model` | KWin installed headers | Display1 |
| `src/compositor/kwin/kwindevelopmentoutputseam` | gated `AddVirtualOutputForTest`/`RemoveVirtualOutputForTest` on `OutputBackend::createVirtualOutput/removeVirtualOutput` | KWin `core/outputbackend.h` (installed **[U]**) | production enablement |
| `src/compositor/kwin/kwinhybridoutputreflow` | group-level reaction to `Workspace::outputsChanged` + geometry pass: one rollback-safe reflow per container; restore-state output remap | existing Hybrid collaborators | topology mutation |
| `src/apps/settings_center/displays/**` | `DisplaysViewModel`, `ArrangementModel`, `DisplaysPage.qml` | `display_client`, Settings1 client, AppShell/Controls | shell |

### Dependency direction (hard rules)

- Platform → compositor only through the Wayland protocol; the plugin never links or calls Display1.
- Shell → `display_client` only; geometry never waits on Display1.
- Settings Center → `display_client` + Settings1 client; never shell or compositor.
- Color1/brightness → `display_client` policy class only (§5 classes).
- No module outside `display_compositor_adapter` includes generated protocol headers; no module outside `display_service` opens the journal.

### Threading, lifetime, error behaviour (public-interface statements required by `AGENTS.md:29-31`)

- Display1: single main thread; Wayland fd via `QSocketNotifier`; all D-Bus asynchronous; no worker threads. Public objects owned by the service; clients hold value snapshots only.
- Errors: closed `DisplayErrorCode` set (`InvalidCandidate`, `StaleRevision`, `TransactionActive`, `CompositorRejected(reason)`, `CompositorUnavailable`, `Timeout`, `Locked`, `TopologyChanged`, `ExternalChange`, `RevertFailed`, `RegistryUnavailable`); every mutation reply carries the initiating epoch/revision.
- Timeouts: apply acknowledgement 5 s (KWin sends `applied`/`failed` **[U]**); observation settle 2 s after `applied`; confirmation default 15 s (5–60, Settings1); revert apply 5 s, three retries (250/500/1000 ms) then `Stuck`.
- Publication: snapshot generations are value-swapped; a rejected or malformed compositor batch never replaces the last valid generation (same rule as `compositor-control-v1.md:132-138`).

### Authorization boundary (addendum item 9)

Same-user session bus; **no security boundary is claimed**, matching the Settings1 manager decision **[B 1787796417]**. Any Wayland client can bind `kde_output_management_v2`, so "single writer" is a QindaQt component policy, not an enforcement: Display1 treats external configuration changes as legitimate newer intent (§5, `ExternalChange`). Residency is required for rollback: the timer and journal live in Display1, not in the Settings Center.

---

## 4. Output identity contract

### Runtime identity (per session)

- `connectorName`: KWin `Output::name()` = `wl_output.name` (v4) = `xdg_output.name` = Compositor1 `Outputs[].name` = `ShellVisibilitySnapshot.outputs[].id` **[R managedwindowregistry.cpp:381; kwinshellvisibilitypublisher.cpp:287]** = `QScreen::name()` in the qualified stack **[R production-surface matrix passes an exact-name bijection, testing-harness.md:171-199]**. Qt documents `QScreen::name()` as "not guaranteed to match … native APIs" and not a unique identifier **[U Qt QScreen]**; QindaQt relies on the *Wayland* behaviour only, which is nested-qualified **[R]** but whose Qt source derivation could not be read **[I]**.
- `compositorUuid`: KWin's `kde_output_device_v2.uuid`, generated by `OutputConfigurationStore::registerOutputs` with `QUuid::createUuid()`, persisted in `kwinoutputconfig.json`, reset on duplicates **[U]**; the protocol says it "should be persistent across restarts" **[U XML]** while KWin's header says it "may change during hotplug events due to display compatibility limitations" **[U backendoutput.h]**. Treat as a *runtime handle* (needed for `set_replication_source`), never as the persistent key.

### Persistent identity `stableId` **[P]** — mirrors KWin's store precedence **[U outputconfigurationstore.cpp findOutputIndex]** so both authorities agree by construction

1. EDID identifier (KWin: manufacturer PNP + product + serial + week + year + model-year **[U edid.cpp]**) non-empty **and unique among connected outputs** → `edid:` + SHA-256/128-bit of the identifier.
2. else raw-EDID hash unique → `edidraw:` + hash.
3. else MST path non-empty and unique → `mst:` + hash(identifier | mstPath).
4. else `conn:` + connectorName (port-bound identity; identical monitors without serials inherit port settings — the same outcome KWin produces).

Rules: **connector/EDID replacement** — same connector, new EDID → new `stableId` (old registry entry kept with `lastSeen`); same monitor on another connector → same `stableId`, aliases follow it. **Cloned/mirrored outputs** keep separate identities; replication is a relationship (`replicationSourceStableId`). **Privacy** — raw EDID bytes and serial numbers never appear in snapshots, logs, diagnostics, or board evidence; snapshots expose `manufacturer`, `model`, `hasSerial`, `physicalSizeMm`, `internal`. **Malformed metadata** — unparsable EDID → rule 4; empty make/model → connector label; strings sanitized to bounded UTF-16. **Collision/ambiguity** — if two connected outputs still produce one id (theoretically impossible under uniqueness checks, defensively handled), suffix `#n` in `outputOrder` order and set `ambiguous=true`; aliases never bind to ambiguous ids. **Alias/remap** — aliases live in Settings1 (`displays.outputAliases`, alias → `stableId`, bounded 32); profile `output` selectors accept `*`, an exact connector name (today), or `@alias` (new); unresolved selectors are *skipped with a diagnostic* rather than failing the solve (requires the shell decision in §12). **Registry** — `displays.configuration` becomes Display1's versioned known-output document (bounded 64 entries, LRU), written only after a confirmed transaction or first-sight detection; write failure → `Degraded(RegistryUnavailable)` without affecting display state.

---

## 5. Display transaction state machine

### Facts the machine is built on **[U]**

- One `kde_output_configuration_v2` object is single-use; `apply` yields `applied` or `failed` (+ `failure_reason` since v12); KWin rejects: already applied, output no longer available, ICC load failure, all outputs disabled, self-mirroring, invalid custom modes, **negative positions on enabled outputs**, positions > 1,000,000, driver rejection.
- On success KWin calls `updateOutputs()` then **`storeConfig()`** — the applied state is persisted immediately, including per-output mode/scale/transform (outputs array) and per-set enabled/position/priority/replication (setups array).
- On hotplug (`outputsQueried`) KWin re-queries its store (`Preexisting` or `Generated`) and applies it itself.
- KWin's virtual backend honours `enabled`, `transform`, `position`, `scale`, `mode`, `priority`, `replicationSource`, `customModes` (not brightness) **[U virtual_output.cpp]**, so the whole class-A machine is nested-testable.

### Classes **[P]**

- **Class A (confirm-gated):** enable/disable, mode, scale, transform, position, priority/primary, replication, HDR/WCG.
- **Class B (policy, immediate, revision-fenced, not journaled):** brightness, dimming, SDR brightness, ICC profile path, VRR policy, RGB range, overscan, DDC-CI allowed, max bpc, EDR, sharpness, auto-rotate policy, custom-mode *definitions*. Color1 and the brightness model use only class B.

### States and transitions

```
Discovering ──first complete device+order batches──▶ Ready(rev N)
Ready ──Stage(candidate, baseRevision=N) valid──▶ Staged(txId)      (no compositor call)
Staged ──Preview(txId)──▶ Applying   : journal pre-image (fsync) → single configuration → apply
Applying ──applied──▶ Observing      : wait until own device events match candidate fingerprint (≤2 s)
Applying ──failed/timeout──▶ Ready   : Rejected(CompositorRejected|Timeout); journal cleared after resnapshot confirms pre-image
Observing ──converged──▶ AwaitingConfirmation(deadline = now + confirmTimeout)   [TransactionChanged]
AwaitingConfirmation ──Confirm(txId)──▶ Committing : registry/policy writes (best effort) → journal cleared → Ready(rev N+k)
AwaitingConfirmation ──Cancel(txId) | deadline | lock | PrepareForSleep(true)──▶ Reverting
Reverting ──applied──▶ Ready : reason ∈ {Cancelled, RevertedByTimeout, Locked, Suspend}
Reverting ──failed×3──▶ Stuck : journal kept, rescue exposed, Degraded
Any active ──topology change (device added/removed)──▶ Reverting(TopologyChanged)  (see rule below)
Any active ──external configuration change──▶ Aborted(ExternalChange): no revert, journal cleared, resnapshot
```

- **Confirmation-timer authority:** Display1, monotonic clock, independent of any UI; every client sees the deadline in `TransactionChanged`. UI countdowns are projections only.
- **Fencing:** `baseRevision` must equal the current snapshot revision at `Stage`; one active transaction; `Confirm/Cancel` accepted from any same-user client by `txId` (the shell overlay and the Settings Center are both valid presenters); owner replacement of the staging client does not cancel the transaction (the timer protects the user).
- **Hotplug during a transaction** (the hardest sub-problem): KWin re-applies its own setup for the *new* set, but per-output properties (mode/scale/transform) previewed on surviving outputs were already stored per output **[U]** and would persist. Rule: wait for KWin's settle (`outputsQueried`-driven batch + 500 ms quiet), then revert **only per-output properties** of surviving outputs to the pre-image; **never** re-apply per-set fields (enabled/position/priority/replication) because the set changed; repeated changes reset the settle window; >10 s of churn → `Unstable`, revert when quiet.
- **Service crash / restart:** on start, a journal in any active state triggers `Reverting(Recovery)` with the same surviving-output rule; the shell's facade activates Display1 at login, so recovery normally happens before the user notices. **Residual risk (documented):** if Display1 is never activated in the next session, KWin restores the unconfirmed preview until the user opens Displays.
- **KWin crash:** the session ends via the supervisor chain **[R compositor-session.md:64-73]**; next login → journal recovery as above.
- **Lock screen / before shell:** `Preview` is refused while locked (policy-grade observation of `org.freedesktop.ScreenSaver` `GetActive/ActiveChanged`, which KScreenLocker provides on the same object the repo's lock monitor uses **[R testing-harness.md:276-280]**; this is policy, not the privacy gate, so no PID authentication is claimed); lock during an active transaction reverts. Before the shell exists transactions are allowed (no presenter; timer still protects).
- **Rescue when the new topology hides the dialog:** (1) shell overlay on **every enabled output** (layer overlay, centred, `KeyboardInteractivityOnDemand` **[U LayerShellQt]**, Enter = Keep, Escape = Revert); (2) shell-owned KGlobalAccel action "Revert display changes" → `CancelActive` (ADR-0009 path **[R]**); (3) Display1 timeout; (4) Settings Center in-page countdown. No path is the timer authority except (3).
- **Suspend:** at `Preview`, Display1 takes a logind `sleep` **delay** inhibitor (`Inhibit("sleep","qindaqt-display","revert unconfirmed display preview","delay")` **[U login1]**, bounded by `InhibitDelayMaxSec=5 s` default **[U logind.conf]**); on `PrepareForSleep(true)` it reverts and releases; on `PrepareForSleep(false)` it treats the following KWin re-query as hotplug (no revision if the fingerprint is unchanged). KWin itself already holds a `sleep` delay inhibitor as "compositor" **[U session_logind.cpp]**.

---

## 6. Atomic downstream reconciliation

### Upstream ordering inside KWin **[U workspace.cpp]**

`applyOutputConfiguration` → backend apply → `updateOutputs()` emits `outputAdded*`, `outputRemoved*`, `outputsChanged`; `desktopResized()` reassigns every window's output by frame centre, calls `checkWorkspacePosition` per window, evacuates tiles, emits `geometryChanged`; priority sort emits `outputOrderChanged`; Wayland `wl_output`/`xdg_output`/`kde_output_device_v2` events are batched with 0 ms single-shot `done` timers. Then `storeConfig`, `updateXwaylandScale()`.

### Epoch/revision contract table **[P]**

| Source | Epoch | Revision | No-op rule | Stale-signal rule | Consumer action |
| --- | --- | --- | --- | --- | --- |
| Compositor1 `Outputs` (1.1) | service epoch (exists for visibility) | `outputGeneration` bumped on `outputsChanged` and per-output geometry/scale/priority change | unchanged canonical inventory → no bump | clients bind `(owner, epoch, generation)`; older owner/epoch rejected | refetch |
| Compositor1 `ShellVisibilitySnapshot` | existing | existing revision | existing | existing (`compositor-control-v1.md:150-168`) | shell reconcile pass |
| Qt screens | none | none | — | shell requires exact bijection with the compositor generation, else safe-visible for that pass | shell reconcile pass |
| Display1 `DisplaySnapshot` | Display1 instance UUID | monotonic per epoch; `liveFingerprint` = hash of canonical (connector, logical rect, scale×1000, transform, enabled, priority, replication) | unchanged fingerprint (e.g. identical re-apply on resume) → no revision | clients reject replies from an older owner/epoch; mutations carry initiating revision | alias/overlay updates only |
| Settings1 `displays.*` | Settings1 epoch | global revision | unchanged value → no change | invalidation hint → scoped re-read **[B ada-ruiz Q3.2]** | Display1 reloads policy; shell schedules a reconcile for aliases |
| Shell controller | — | existing controller revision | in-place update when static roles equal (`layer_shell_surface_backend.cpp:239-252`) | prior set retained on failure | — |
| Hybrid topology | — | topology revision (reflow does not bump) | — | — | group reflow per container (D5) |

### Ordering guarantees

1. **Geometry never waits on Display1.** The shell reacts to Qt + Compositor1 exactly as today; a mismatch produces one bounded safe-visible pass that converges on the next generation. Display1's `TransactionChanged(AwaitingConfirmation)` is emitted only after Display1 observed convergence, so overlays are created on the *new* topology through the shell's normal pass; an overlay whose output vanished is dismissed by the layer-shell `closed` event **[U layershell_v1.cpp]** and recreated by set replacement.
2. **Registry/policy writes happen only at Commit**, after live convergence; a `SettingsChanged` for aliases is just another generation input to the pure alias resolver, so no partial shell state is possible.
3. **Layout-profile preview**: an editor session binds one inventory; any Display1 fingerprint change or shell inventory change cancels/rebuilds it (existing rule `layout-profiles.md:117-121` becomes executable through the facade).
4. **Hybrid**: KWin's per-window pass runs first; the D5 reflow runs on the next event-loop turn, computes the group's target outer frame with the existing relative-centre mapping from the old to the new placement area, re-solves every page, and applies one rollback-safe `reflowContainerWithContext` (`kwinhybridreflow.cpp:225-230`); if the anchor's output vanished, the group adopts the output KWin assigned to the representative. Independent restore snapshots whose `outputId` no longer exists are remapped at release/detach time with the same rule (topology valid, geometry approximate — the product truth).
5. **No-op transitions**: resume with identical outputs, KWin re-applying the same stored setup, or a `Stage` whose diff is empty produce no revision, no overlay, and no shell work beyond the existing in-place path.

---

## 7. Geometry and DPI rules

- **Logical geometry authority:** KWin `LogicalOutput::geometry()` = `Rect(position, pixelSize / scale)` (Qt nearest rounding) **[U output.cpp]**; `xdg_output` sends `round(geometryF)` **[U xdgoutput_v1.cpp]**; Compositor1 visibility uses the same integral rect **[R kwinshellvisibilitypublisher.cpp:284-287]**. Display1 must take logical geometry **from `xdg_output`**, never divide itself; a unit table pins parity: 1920×1080 → 1536×864 @1.25, 1280×720 @1.5; 1920×1200 → 1536×960 @1.25; 2560×1440 → 2048×1152 @1.25, **1707×960 @1.5 (non-integral, rounded)**.
- **Integral-extent preference:** the validator flags non-integral extents as warnings (the harness already rejects them for the common-argument backend **[R compositor-session.md:313-315]**); UI offers 1.0–3.0 in 0.25 steps plus KWin's suggested value (KWin rounds its own defaults to 0.20 steps and snaps <1.20 to 1 **[U]**); `displays.fractionalScaling=false` restricts to integers.
- **Positions:** integer logical; enabled outputs must not overlap; gaps allowed (warning). **KWin 6.6.5 rejects negative positions of enabled outputs and values > 1,000,000 [U]**, so Display1 *normalizes* candidates to a (0,0) origin before staging. Consequence for the required matrix: "negative coordinates" (`testing-harness.md:549-552`) cannot be produced on KWin nested sessions; keep negative-coordinate coverage as unit/fake-backend contract tests for the shell and Hybrid math (they already exist: `layout-profiles.md:104-107`) and reword the harness page (release question).
- **Transforms:** logical size transposed for 90/270 kinds **[U orientateSize]**; Display1 uses the Compositor1 vocabulary (`normal`, `rotate-90`, …, `flip-x-270`) and must map the scenario vocabulary (`flipped*`) in the harness driver.
- **Mirroring:** `set_replication_source` (since v13 ≤ 19) is supported **[U]**; KWin computes the mirror's scale and offset in `applyMirroring` **[U]**. Whether a replicating output remains in `Workspace::outputOrder()`/`wl_output` (and thus appears to the shell as a second output with its own logical rect) is **unverified [I]** and is the first assertion of the nested mirror row; the shell solver must never see two outputs with identical rects unless that is the documented KWin behaviour.
- **Cross-output grouped-window motion:** frames are continuous logical coordinates; KWin assigns each member's output by centre **[U]**; Hybrid chrome samples DPR from the representative's output (`kwinhybridsession.cpp:456-457`); a group may straddle a scale boundary at the logical level with physical seams accepted.
- **Work areas:** per-output logical exclusive zones (existing); Display1 has no work-area concept; Hybrid maximize uses `clientArea(MaximizeArea)` (existing).
- **Font DPI separation:** Wayland logical DPI is fixed; text size comes only from `fonts.pointSize` through QST/AppBootstrap **[B 1787854166 Q2.2, 1787854245]** and `accessibility.textScale`; display scale never changes point size; the session must not set `QT_WAYLAND_FORCE_DPI`.
- **Client/server decorations:** QindaDecoration (SSD) margins are logical and included in Hybrid constraints (`kwinhybridplatform.cpp:139-160`); CSD clients receive `wp_fractional_scale_v1` (KWin serves v1, `round(scale×120)` **[U]**); XWayland gets one global scale via `updateXwaylandScale()` **[U]**, so XWayland members of a mixed-DPI group are logically exact but physically resampled on the other output.
- **Shell fractional-scale hazard [I]:** Qt documents that on Wayland "Qt reads `wl_output::scale`, which is restricted to integer values" **[U Qt High-DPI]**, KWin sends `ceil(scale)` **[U]**, and the shell requires `QScreen::devicePixelRatio() == compositor scale` (`output_inventory_matcher.cpp:70-73`, `layer_shell_surface_backend.cpp:228-230`). At 1.25 the shell would therefore reject the compositor generation and run permanently safe-visible, and layer surfaces would render at buffer scale 2 unless Qt's window-level fractional path engages. This is the first thing slice D4 must measure; the proposed fix keeps name+geometry bijection and treats compositor scale as the only scale metadata.

---

## 8. Hotplug, lid, and suspend policy

| Event | KWin 6.6.5 behaviour **[U]** | QindaQt v1 default **[P]** | User override (later slice) |
| --- | --- | --- | --- |
| Never-seen output set | `Generated` config: scale heuristic (internal with lid: 125 DPI target/min 800 px; external 96 DPI; TV >500 mm: 30.5 DPI; clamp 1–3, 0.2 steps), left-to-right placement, stored; with an internal panel and exactly two outputs it calls `org.kde.kscreen.osdService.showActionSelector` | Display1 registers identities, no auto-apply; shell facade shows a non-blocking "Arrange displays" prompt linking to the Displays route | `displays.autoArrangePrompt` |
| Known set reconnect / dock | `Preexisting` setup restored (per-set position/enabled/priority/replication; per-output mode/scale/transform) | nothing unless fingerprint changed; registry `lastSeen` | — |
| Primary transfer | first in priority order (`kde_output_order_v1`; `outputOrder()`) | optional class-A *policy* transaction only when `displays.primaryOutput` names a present, non-primary output (default empty = compositor) | `displays.primaryOutput` |
| Missing output for a profile panel | — | named/alias selectors skipped with diagnostic (shell decision §12) | — |
| Lid close | `generateLidClosedConfig`: internal disabled when another enabled output exists, else kept; setups keyed by `lidClosed` | no inhibitor; logind defaults apply (`HandleLidSwitch=suspend`, `HandleLidSwitchDocked=ignore` when docked or >1 display, `HoldoffTimeoutSec=30 s`) **[U]** | Power1 takes `handle-lid-switch` only when configured **[U inhibitor doc]**; overriding KWin's internal-panel disable needs a downstream patch decision (§12) |
| Suspend / resume | KWin holds a `sleep` delay inhibitor; DRM re-probes on `deviceResumed`/udev `change` | Display1: revert active preview before sleep; treat resume re-query as hotplug; no revision if unchanged | — |
| Repeated changes during a transaction | store re-applies per set | settle window, per-output-only revert, `Unstable` after 10 s | — |
| Lock screen | — | refuse `Preview`; revert active | — |
| Before shell ready | — | allowed; timer-only protection | — |

Rate limits: one transaction; hotplug bursts coalesced 500 ms; snapshot publication debounced 16 ms (visibility-client precedent **[R compositor_visibility_client.h:16-20]**).

---

## 9. Settings and UI contracts

### Settings1 keys **[P]** (no cross-key invariant by design → single-key commits suffice)

| Key | Type / default | Owner of semantics | Writer |
| --- | --- | --- | --- |
| `displays.fractionalScaling` (exists) | bool / true | Display1 validator + UI | Settings Center |
| `displays.primaryOutput` (exists) | string `stableId` / "" | Display1 policy | Settings Center |
| `displays.hdrPolicy` (exists) | enum off/automatic/on | Display1 class-A default on first sight only | Settings Center |
| `displays.configuration` (exists, object) | Display1 registry document `{schemaVersion, outputs{stableId→{label, alias?, lastSeen, lastConnector, manufacturer, model, internal}}}` bounded 64 | Display1 (validated by `display_identity`) | Display1 only |
| `displays.confirmTimeoutSeconds` (new) | int 5–60 / 15 | Display1 timer | Settings Center |
| `displays.outputAliases` (new) | object alias→`stableId`, ≤32 | shell resolver + Display1 | Settings Center |
| `displays.autoArrangePrompt` (new) | bool / true | shell facade | Settings Center |
| `displays.lidClosePolicy` (new, reserved) | enum `compositor-default` only in v1 | Power1/Display1 later | — |

Migration: new keys require a schema revision with defaults (v2 is Settings1's current candidate **[B]**); Display1 tolerates `UnknownKey` for new keys against an older schema as `Degraded(PolicyUnavailable)` **[B ada-ruiz 1787854099]**. Multi-key atomicity is deliberately not required; if the Settings1 batch client lands later, pages may group writes.

### Displays page view model **[P]** (maps provider truth to the accepted vocabulary **[B 1787853959, 1787854167]**)

States: `Loading` · `Ready` · `Staging` (validation diff shown) · `Previewing(countdown)` · `Confirming` · `Reverting` · `Conflict` (outputs changed since base revision: refresh, explicit retry) · `Unavailable` (Display1 absent: aliases/policy only, clearly labelled) · `Degraded` (registry or capability missing: control hidden with reason) · `Stuck` (revert failed: rescue instructions, retry). No cached value is presented as current after owner loss; uncertain mutations are never replayed.

Accessibility: non-colour output labels and spatial descriptions ("DP-1, 27-inch, right of eDP-1"); keyboard arrangement (arrows move 8 logical px, Shift+arrows snap to neighbour edges, Ctrl+arrows swap); rotation/scale/mode as combo boxes; "Identify" shows overlay labels on every output; countdown in an assertive live region; focus returns to **Keep**; Escape reverts; all strings localized.

Preview/confirm/revert UX: Apply → immediate preview + overlay countdown on all displays + in-page countdown → Keep / Revert → result banner (kept, reverted, reverted by timeout, reverted by hotplug).

Deep links: `qindaqt-settings --page displays` and `--page displays --output <stableId>` through the route registry (Settings Center owns routes **[B 1787796417, 1787853958]**); the shell prompt uses only that fixed route.

Shell facade: read-only alias map + transaction state, `confirm()`/`revert()`/`identify()` requests; QML never receives the client.

---

## 10. Verification matrix

### Deterministic tiers **[Q-det]** (names follow existing registry conventions)

| Tier | Test | Proves |
| --- | --- | --- |
| Unit | `qindaqt.display-identity-*` | EDID fixtures: unique serial, duplicate identical monitors (no serial), malformed EDID, MST paths, connector rename, KWin-precedence parity, privacy (no serial in output), ambiguity suffixing |
| Unit | `qindaqt.display-topology-*` | overlap/gap/normalization, >1e6 bound, transform transposition, integral-extent warnings, logical-rounding parity table (§7), mirror self-reference, all-disabled, primary rules, diff/no-op |
| Unit | `qindaqt.display-transaction-*` | every transition with fake clock/port: apply timeout → Uncertain, confirm, cancel, deadline revert, revert failure → Stuck, hotplug mid-preview per-output-only revert, external change abort, suspend abort, journal write/recovery, class B bypass |
| Private D-Bus | `qindaqt.display-service-*` | activation, owner replacement/epoch, stale base revision → Conflict, lock refusal, Settings1 fake for registry/policy, fake `org.freedesktop.login1` `PrepareForSleep` and delay inhibitor FD lifecycle, journal recovery on restart, bus-loss exit |
| Private D-Bus | `qindaqt.display-client-*` | debounce, timeout/backoff, stale replies, uncertain results, no replay, `service_availability` mapping |
| Nested KWin (virtual, dev mode) | `display.nested.single-{1080p,wuxga,1440p}-{100,125,150}` | Display1 snapshot equals Compositor1 `Outputs` and `QScreen` inventory; scale transactions confirm and timeout-revert; KWin store file in the isolated XDG config reflects applied then reverted values |
| Nested KWin | `display.nested.portrait-wuxga` | rotate-90 → 1200×1920 logical; panels and a group on the portrait output |
| Nested KWin | `display.nested.mixed-dpi`, `…gapped`, `…primary-transfer`, `…mirror`, `…custom-mode` | second output via `AddVirtualOutputForTest`; 1080p + 1440p@1.25; gapped placement; priority swap; replication (first assertion: presence of the mirror in `wl_output`); `set_custom_modes` 2560×1440@120 |
| Nested KWin | `display.nested.hotplug-{add,remove,reorder}`, `…hotplug-during-preview`, `…service-restart-recovery` | dev seam add/remove; per-output-only revert after topology change; journal recovery |
| Nested KWin (shell) | `shell.production-surface.{1080p,wuxga,1440p}-{125,150}`, `shell.production-surface.hotplug-atomic` | fractional-scale bijection fix; no partially published `(panelId,outputId)` set across a hotplug generation (count mapped roles per output between generations); named/alias selector skip |
| Nested KWin (Hybrid) | `compositor.hybrid-output-{scale,rotate,remove}-reflow`, `…restore-remap` | three-member group stays one unit; single reflow; release restores members with remapped output |
| Nested KWin (clients) | `display.nested.xwayland-csd-group` | XWayland probe + frameless (CSD stand-in) Qt probe grouped across mixed DPI; logical frames exact |
| Optional oracle | `kscreen-doctor --json` (libkscreen `src/doctor/doctor.cpp` **[U]**) | independent read of the same protocol; test-only dependency, never a product one |

Constraint recorded: the dev seam can create outputs with name/size/scale only (`OutputBackend::createVirtualOutput` signature **[U]**; `VirtualBackend::addOutput(OutputInfo)` with EDID/MST is not an installed header **[U src/CMakeLists.txt]**), so EDID/MST identity is unit-tested, not nested.

### Physical qualification **[Q-hw]** (release lane, reported separately, never implied by the above)

Intel, AMD, NVIDIA (proprietary and nouveau), hybrid graphics/PRIME secondary-GPU outputs, DP-MST docks and USB-C re-plug storms, two identical monitors without serials, KVM/EDID-less paths, DDC-CI brightness, HDR/WCG panels, VRR, 4K@2×, TV heuristics (>500 mm), laptop lid close/open with and without external output, suspend/resume with outputs changed while asleep, touchscreen mapping after rotation, real GTK/Electron CSD, SDL/Wine/Java clients.

---

## 11. Vertical delivery slices (dependency-ordered)

| Slice | Outcome | Exact path ownership | Wiki/ADR | Acceptance evidence | Collision points | Owner (lane → suggested worker) |
| --- | --- | --- | --- | --- | --- | --- |
| **D0** Compositor1 1.1 + dev output seam | `Outputs` gains `outputGeneration`, `uuid`, `priority`, `physicalSizeMm`, `manufacturer`, `model`; gated `AddVirtualOutputForTest`/`RemoveVirtualOutputForTest` | `src/compositor/kwin/kwinoutputinventory.{h,cpp}`, `src/compositor/kwin/kwindevelopmentoutputseam.{h,cpp}`, `tests/compositor/…output…`; coordination: `compositor/dbus/org.qindaqt.Compositor1.xml`, `src/compositor/src/controlcodec.cpp`, `compositor/dbus/service.json` | `reference/compositor-control-v1.md`, `architecture/compositor-session.md` | parity test; production pre-parse `control-disabled`; nested add/remove shows `outputsQueried` effects in `Outputs`/`ShellVisibilitySnapshot` | Hybrid owner (`managedwindowregistry.cpp` extraction) | compositor/Hybrid → new implementer (manager-assigned); reviewer: Rowan Ivers |
| **D1** Pure display modules | protocol, identity, topology, transaction | `src/services/display_{protocol,identity,topology,transaction}/**`, `tests/services/display_{…}/**` | new `architecture/display-service.md`, `reference/display1-v1.md`, ADR-0014/0015 (§13) | unit rows above, Debug+Release, source shape | none | platform → new implementer; design reviewer Samira Cole |
| **D2** Display1 service + adapter (**hardest first problem**) | resident service, pinned XML client, journal, delay inhibitor, nested class-A transactions | `src/services/display_compositor_adapter/{protocols/*.xml,manifest.json,…}`, `src/services/display_service/**`, `tests/services/display_service/**`, `tests/display/nested/**`; coordination: `src/CMakeLists.txt`, `tests/CMakeLists.txt`, install/activation data | same pages + ADR-0016 (vendored protocol pin) | private-D-Bus rows; nested single-output and hotplug-during-preview rows; recovery row | needs D0 seam for hotplug rows; `tests/CMakeLists.txt` | platform → same implementer as D1; reviewer Talia North |
| **D3a** Settings keys | schema revision with new `displays.*` keys and registry-object validation hook | Settings1-owned paths (`data/settings/schema-*.json`, `src/settings/**`, `src/services/settings_*`) | `architecture/settings-service.md`, ADR-0012 consequence note | migration + malformed tests | **Ada Ruiz's paths — must be her slice after integration** | Settings1 → Ada Ruiz |
| **D3b** display_client + availability | public client, `service_availability` producer mapping | `src/services/display_client/**`, `tests/services/display_client/**` | `reference/display1-v1.md` client section | private-D-Bus client rows | `src/sdk/service_availability` creation is Samira-gated | platform → D1/D2 implementer |
| **D4** Shell integration | fractional-scale bijection fix; alias resolver; skip-unresolved selector; overlay surface; confirmation/identify overlays; revert shortcut | `src/shell_orchestration/…/output_alias_resolver.*`, `src/shell_surface/…/layer_shell_overlay_surface.*`, `src/shell/runtime/display{facade,confirmationcontroller,revertshortcut}.*`, `src/shell/qml/DisplayConfirmationOverlay.qml`, tests | `shell/panel-surfaces.md`, `shell/layout-profiles.md`, ADR-0007 amendment (ADR-0017) | `shell.production-surface.*-125/150`, hotplug-atomic, offscreen overlay accessibility | `shellruntimeapplication.cpp` (shared runtime), `mkdocs.yml` | shell/customization → first post-Settings1 shell owner (unassigned) |
| **D5** Hybrid output reflow | group reflow on geometry/scale/removal; restore remap | `src/compositor/kwin/kwinhybridoutputreflow.{h,cpp}`, `kwinhybridplatform.cpp` (remap rule), tests | `architecture/hybrid-constraints.md`, `hybrid-topology.md` | Hybrid nested rows; fake-platform rollback | Hybrid session composition file (`kwinhybridsession.cpp`) | compositor/Hybrid → D0 implementer |
| **D6** Displays page | view model, arrangement model, page, deep link | `src/apps/settings_center/displays/**`, `tests/apps/settings_center/displays/**` | settings-service IA section | offscreen state/accessibility rows | route registry (native-app owner), AppShell S3 | native app → Juno Park's lane implementer after S3 |
| **D7** Policy & class-B API | primary/HDR policy transactions, registry write-back, new-output prompt, typed class-B methods for Color1/brightness | `display_service`, `display_client` additive | `display-service.md` | private-D-Bus + nested primary-transfer | Color1/brightness consumers | platform |
| **D8** Release | packaging (activation file, user unit), Plasma KScreen OSD exclusion/coexistence, hardware matrix, footprint budget (proposed ≤ 10 MiB PSS idle, no periodic wakeups) | packaging/CI paths | `testing-harness.md` hardware section, harness wording fixes (§7 negative coordinates; `backends.py` notes) | **[Q-hw]** reports | — | release owner (unassigned) |

**Hardest first implementation problem (do not implement here):** D2's proof that `Preview → hotplug → Revert` converges to the pre-image for surviving outputs *without* reading `kwinoutputconfig.json` and *without* racing KWin's own re-application on `outputsQueried`, given that KWin persists previewed per-output properties immediately. Its acceptance is the `display.nested.hotplug-during-preview` and `…service-restart-recovery` rows.

---

## 12. Cross-lane board questions

Each block follows the board contract **[B 1787853412]** (From: display/output analysis; To: named lane).

**Q-P1 → Platform services (Samira Cole / Display1 implementer).** *Decision:* whether users get display configuration through a direct protocol client or libkscreen. *Interface:* `display_compositor_adapter` on pinned XMLs vs `KScreen::Config/SetConfigOperation`. *Default:* direct adapter; *alternatives:* libkscreen in-process (`KSCREEN_BACKEND_INPROCESS` **[U]**), libkscreen out-of-process (`kscreen_backend_launcher`, `org.kde.KScreen`). *Paths:* `src/services/display_compositor_adapter/**`. *Continue:* yes for D1. *Evidence requested:* acceptance of the pin rule (plasma-wayland-protocols 1.20.0 = mgmt 19 / device 20) and of `kscreen-doctor` as test-only.
**Q-P2 → Platform (Power1).** *Decision:* who holds sleep/lid inhibitors. *Interface:* Display1 `sleep` delay inhibitor during previews only; Power1 owns `handle-lid-switch` later. *Default:* as stated; *alternative:* Power1 proxies all inhibitors. *Paths:* `display_service` logind adapter. *Continue:* yes. *Evidence:* Power1 plan confirmation.
**Q-P3 → Platform (Color1/brightness).** *Decision:* single mutation transport. *Interface:* Display1 class-B `ApplyPolicy(candidate, baseRevision)`; no `Brightness1`. *Default:* Display1 owns brightness/DDC-CI/ICC/HDR/WCG/VRR/auto-rotate transport (KWin exposes them on one configuration object **[U]**). *Continue:* yes. *Evidence:* Color1 design amended to consume the typed API.

**Q-C1 → Compositor/Hybrid.** *Decision:* deterministic hotplug in nested sessions. *Interface:* gated `AddVirtualOutputForTest(name,width,height,scale)`/`RemoveVirtualOutputForTest(name)` on `OutputBackend::createVirtualOutput/removeVirtualOutput` **[U installed header]**. *Default:* add to Compositor1 1.1; *alternative:* none deterministic. *Paths:* D0. *Continue:* D1/D3 yes; D2 hotplug rows wait. *Evidence:* parity test and production rejection.
**Q-C2 → Compositor/Hybrid.** *Decision:* grouped windows stay grouped across scale/rotation/removal. *Interface:* D5 reflow + restore remap rule ("substitute current group output, relative-centre map"). *Default:* as stated; *alternative:* release the group on output loss. *Paths:* D5. *Continue:* yes. *Evidence:* nested rows; confirmation whether `Workspace::geometryChanged`/`desktopResized` ordering is observable from the plugin (KWin `checkWorkspacePosition` specifics were not readable **[I]**).
**Q-C3 → Compositor.** *Decision:* Plasma OSD activation from KWin's generated-config path (`org.kde.kscreen.osdService.showActionSelector` **[U]**, activatable on this host). *Default:* no downstream patch; release lane excludes/masks the service; *alternative:* first downstream patch (ADR-0001 series becomes non-empty). *Continue:* yes. *Evidence:* decision record.

**Q-S1 → Shell/customization owner (routed via manager).** *Decision:* fractional-scale outputs must not run in permanent safe-visible fallback. *Interface:* `OutputInventoryMatcher` and `screenMatchesConfiguration` drop DPR equality; keep name+geometry. *Default:* as stated. *Paths:* `src/shell_orchestration/src/output_inventory_matcher.cpp`, `src/shell_surface/src/layer_shell_surface_backend.cpp`. *Continue:* yes. *Evidence:* nested `shell.production-surface.1080p-125` before and after.
**Q-S2 → Shell.** *Decision:* undocking with a user profile naming a missing output must not remove all panels. *Interface:* unresolved named/`@alias` selectors are skipped with a diagnostic (ADR-0007 amendment). *Default:* skip; *alternative:* current fail-closed. *Paths:* `src/shell_layout`/`shell_orchestration`, `docs/wiki/shell/panel-surfaces.md:88-93`. *Continue:* yes. *Evidence:* nested hotplug-atomic row.
**Q-S3 → Shell.** *Decision:* confirmation/identify overlays on every enabled output with on-demand keyboard focus and a KGlobalAccel revert action. *Interface:* `LayerShellOverlaySurface`, `DisplayFacade`. *Default:* overlay + shortcut; *alternative:* Settings Center only. *Continue:* after D3b. *Evidence:* offscreen accessibility + nested mapping.

**Q-A1 → Settings1 (Ada Ruiz).** *Decision:* new `displays.*` keys. *Interface:* schema revision (v3?) or additive within v2; bounded object key `displays.configuration` for the registry. *Default:* revision with defaults, Display1-validated object; no multi-key requirement. *Paths:* Ada's owned paths. *Continue:* yes (Display1 tolerates `UnknownKey`). *Evidence:* schema/migration tests and bound sizes.

**Q-N1 → Native app/design (Juno Park, Mara Voss).** *Decision:* Displays page states, arrangement keyboard model, countdown live region, deep link `--page displays`. *Interface:* `DisplaysViewModel` mapping (§9), `DegradedNotice` reasons, overlay tokens. *Default:* as §9. *Continue:* after S3 and Settings1 integration. *Evidence:* offscreen rows and route registration.

**Q-R1 → Release owner.** *Decisions:* (a) packaging of Display1 (activation file, user unit); (b) exclusion or documented coexistence of `kscreen_osd_service`, `kscreen_backend_launcher`, and kded's kscreen module (which still writes its own configs on Wayland **[U]**) in QindaQt sessions; (c) libkscreen as test-only dependency; (d) Qt 6.11.1 fractional-scale behaviour on the qualified stack; (e) harness wording: negative coordinates cannot be nested-tested on KWin 6.6.5; `backends.py` notes; (f) hardware matrix and footprint budget. *Continue:* yes. *Evidence:* release checklist entries.

---

## 13. Decision ledger

### ADRs to add (numbers subject to manager allocation; 0012 and 0013 are reserved on the board **[B]**)

- **ADR-0014** Display1 owns display transactions over KWin's output-management protocol; KWin's store remains the restore authority; service-owned confirmation timer and journal.
- **ADR-0015** Persistent output identity, alias, and registry contract (mirrors KWin matching precedence; privacy rules).
- **ADR-0016** Vendored, pinned plasma-wayland-protocols client generation for platform services (MIT-CMU XML licence **[U]**; shared later by Clipboard1's `ext-data-control`).
- **ADR-0017** Amends ADR-0007's consequence that `QScreen::name()` is the runtime identifier and named targets fail: adds `@alias` selectors, skip-unresolved policy, and compositor-scale-only metadata (partial supersession).
- Later, if lid overrides require it: an ADR making the KWin patch series non-empty (ADR-0001 consequence).

### Rejected alternatives

libkscreen as the integration (lockstep Plasma pin, plugin-loaded backend, integer ids, no transaction semantics); in-plugin output mutation (no authentication, ABI-pinned growth, cannot survive UI loss); a general `Platform1` process; reading/writing `kwinoutputconfig.json`; Settings1 as topology authority or desired-state re-applier (double apply, fights KWin); claiming `org.kde.kscreen.osdService`; UI-owned or shell-owned confirmation timers; auto-commit on timeout; making Display1 an essential supervisor child.

### Bounded open questions

1. Qt 6.11.1 `QScreen::devicePixelRatio()`/`name()` derivation under fractional scale (sources unreadable in this run) — resolved by D4's first nested measurement.
2. Visibility of replicating outputs in `wl_output`/`outputOrder()` — resolved by the nested mirror row.
3. Exact KWin `checkWorkspacePosition` clamping — resolved by D5's nested rows; not needed for the design.
4. Lid override without a downstream patch — Power1/compositor decision (Q-C3, Q-P2).
5. Settings schema revision policy for additive keys (Q-A1).
6. Display1 start timing if the shell facade is disabled (residual journal risk) — release decision.

### Manager acceptance checklist

- [ ] Runtime model verified from the raw initialization event; product worktree unchanged (this run edited nothing).
- [ ] §1 boundary accepted; ADR numbers allocated; ADR-0007 amendment scoped.
- [ ] Q-S1 measured before any shell or Display1 code relies on fractional-scale bijection.
- [ ] D0 assigned first (unblocks D2 hotplug rows and D5); D1/D3b may start in parallel on unique paths.
- [ ] D3a scheduled as an Ada-owned slice after Settings1 integration; no platform worker edits Settings1 paths.
- [ ] Release owner answers Q-R1(b) before any nested evidence is interpreted as session behaviour on hosts with Plasma packages.
- [ ] Every slice handoff separates **[Q-det]** from **[Q-hw]** exactly as §10.

---

## Sources

Repository paths are cited inline. Upstream and documentation sources used:

- KWin 6.6.5: [workspace.cpp](https://invent.kde.org/plasma/kwin/-/blob/v6.6.5/src/workspace.cpp), [outputconfigurationstore.cpp](https://invent.kde.org/plasma/kwin/-/blob/v6.6.5/src/outputconfigurationstore.cpp), [core/output.h](https://invent.kde.org/plasma/kwin/-/blob/v6.6.5/src/core/output.h), [core/output.cpp](https://invent.kde.org/plasma/kwin/-/blob/v6.6.5/src/core/output.cpp), [core/backendoutput.h](https://invent.kde.org/plasma/kwin/-/blob/v6.6.5/src/core/backendoutput.h), [core/backendoutput.cpp](https://invent.kde.org/plasma/kwin/-/blob/v6.6.5/src/core/backendoutput.cpp), [core/outputconfiguration.h](https://invent.kde.org/plasma/kwin/-/blob/v6.6.5/src/core/outputconfiguration.h), [core/outputbackend.h](https://invent.kde.org/plasma/kwin/-/blob/v6.6.5/src/core/outputbackend.h), [core/outputbackend.cpp](https://invent.kde.org/plasma/kwin/-/blob/v6.6.5/src/core/outputbackend.cpp), [core/session.h](https://invent.kde.org/plasma/kwin/-/blob/v6.6.5/src/core/session.h), [core/session_logind.cpp](https://invent.kde.org/plasma/kwin/-/blob/v6.6.5/src/core/session_logind.cpp), [lidswitchtracker.cpp](https://invent.kde.org/plasma/kwin/-/blob/v6.6.5/src/lidswitchtracker.cpp), [utils/edid.cpp](https://invent.kde.org/plasma/kwin/-/blob/v6.6.5/src/utils/edid.cpp), [wayland/outputmanagement_v2.cpp](https://invent.kde.org/plasma/kwin/-/blob/v6.6.5/src/wayland/outputmanagement_v2.cpp), [wayland/outputdevice_v2.cpp](https://invent.kde.org/plasma/kwin/-/blob/v6.6.5/src/wayland/outputdevice_v2.cpp), [wayland/output.cpp](https://invent.kde.org/plasma/kwin/-/blob/v6.6.5/src/wayland/output.cpp), [wayland/xdgoutput_v1.cpp](https://invent.kde.org/plasma/kwin/-/blob/v6.6.5/src/wayland/xdgoutput_v1.cpp), [wayland/layershell_v1.cpp](https://invent.kde.org/plasma/kwin/-/blob/v6.6.5/src/wayland/layershell_v1.cpp), [wayland/fractionalscale_v1.cpp](https://invent.kde.org/plasma/kwin/-/blob/v6.6.5/src/wayland/fractionalscale_v1.cpp), [wayland/CMakeLists.txt](https://invent.kde.org/plasma/kwin/-/blob/v6.6.5/src/wayland/CMakeLists.txt), [wayland/protocols/wlr-layer-shell-unstable-v1.xml](https://invent.kde.org/plasma/kwin/-/blob/v6.6.5/src/wayland/protocols/wlr-layer-shell-unstable-v1.xml), [backends/virtual/virtual_backend.h](https://invent.kde.org/plasma/kwin/-/blob/v6.6.5/src/backends/virtual/virtual_backend.h), [backends/virtual/virtual_backend.cpp](https://invent.kde.org/plasma/kwin/-/blob/v6.6.5/src/backends/virtual/virtual_backend.cpp), [backends/virtual/virtual_output.cpp](https://invent.kde.org/plasma/kwin/-/blob/v6.6.5/src/backends/virtual/virtual_output.cpp), [backends/drm/drm_backend.cpp](https://invent.kde.org/plasma/kwin/-/blob/v6.6.5/src/backends/drm/drm_backend.cpp), [main_wayland.cpp](https://invent.kde.org/plasma/kwin/-/blob/v6.6.5/src/main_wayland.cpp), [CMakeLists.txt](https://invent.kde.org/plasma/kwin/-/blob/v6.6.5/CMakeLists.txt), [src/CMakeLists.txt](https://invent.kde.org/plasma/kwin/-/blob/v6.6.5/src/CMakeLists.txt), [window.cpp](https://invent.kde.org/plasma/kwin/-/blob/v6.6.5/src/window.cpp) (partially readable).
- plasma-wayland-protocols: [kde-output-management-v2.xml v1.20.0](https://invent.kde.org/libraries/plasma-wayland-protocols/-/blob/v1.20.0/src/protocols/kde-output-management-v2.xml), [kde-output-device-v2.xml v1.20.0](https://invent.kde.org/libraries/plasma-wayland-protocols/-/blob/v1.20.0/src/protocols/kde-output-device-v2.xml), [master management](https://invent.kde.org/libraries/plasma-wayland-protocols/-/blob/master/src/protocols/kde-output-management-v2.xml), [master device](https://invent.kde.org/libraries/plasma-wayland-protocols/-/blob/master/src/protocols/kde-output-device-v2.xml), [kde-output-order-v1.xml](https://invent.kde.org/libraries/plasma-wayland-protocols/-/blob/master/src/protocols/kde-output-order-v1.xml).
- libkscreen 6.6.5: [src/output.h](https://invent.kde.org/plasma/libkscreen/-/blob/v6.6.5/src/output.h), [src/config.h](https://invent.kde.org/plasma/libkscreen/-/blob/v6.6.5/src/config.h), [src/backendmanager_p.h](https://invent.kde.org/plasma/libkscreen/-/blob/v6.6.5/src/backendmanager_p.h), [src/backendmanager.cpp](https://invent.kde.org/plasma/libkscreen/-/blob/v6.6.5/src/backendmanager.cpp), [backends/kwayland/waylandoutputdevice.cpp](https://invent.kde.org/plasma/libkscreen/-/blob/v6.6.5/backends/kwayland/waylandoutputdevice.cpp), [backends/kwayland/waylandconfig.cpp](https://invent.kde.org/plasma/libkscreen/-/blob/v6.6.5/backends/kwayland/waylandconfig.cpp), [src/doctor/doctor.cpp](https://invent.kde.org/plasma/libkscreen/-/blob/v6.6.5/src/doctor/doctor.cpp).
- kscreen 6.6.5: [kded/daemon.cpp](https://invent.kde.org/plasma/kscreen/-/blob/v6.6.5/kded/daemon.cpp), [CMakeLists.txt](https://invent.kde.org/plasma/kscreen/-/blob/v6.6.5/CMakeLists.txt).
- LayerShellQt 6.6.5: [src/interfaces/window.h](https://invent.kde.org/plasma/layer-shell-qt/-/blob/v6.6.5/src/interfaces/window.h).
- Qt 6.11 documentation: [QScreen](https://doc.qt.io/qt-6/qscreen.html), [High DPI](https://doc.qt.io/qt-6/highdpi.html), [QNativeInterface::QWaylandScreen](https://doc.qt.io/qt-6/qnativeinterface-qwaylandscreen.html), [Wayland and Qt](https://doc.qt.io/qt-6/wayland-and-qt.html), [Fractional scale attribution (Qt Wayland Compositor)](https://doc.qt.io/qt-6/qtwaylandcompositor-attribution-fractional-scale-v1.html); non-normative observation: [Qt Forum: fractional scaling reported as integer by QScreen](https://forum.qt.io/topic/160425/wayland-and-fractional-screen-scaling-reported-as-integer-by-wayland-info-and-q-screen); search context: [KDE fractional scaling week](https://pointieststick.com/2022/12/16/this-week-in-kde-wayland-fractional-scaling-oh-and-we-also-fixed-multi-screen/), [wayland-protocols 1.31 news](https://news.itsfoss.com/wayland-protocols-fractional-scaling/), [Hyprland issue #2032](https://github.com/hyprwm/Hyprland/issues/2032), [Mozilla bug 1767142](https://bugzilla.mozilla.org/show_bug.cgi?id=1767142), [VLC commit note](https://www.mail-archive.com/vlc-commits@videolan.org/msg68325.html), [KDE bug 466373](https://www.mail-archive.com/kde-bugs-dist@kde.org/msg785082.html), [Linux Junkies guide](https://linuxjunkies.org/guides/fix-fractional-scaling-on-wayland), [Qt development list thread](https://www.mail-archive.com/development@qt-project.org/msg44327.html), [flameshot PR #3869](https://github.com/flameshot-org/flameshot/pull/3869), [qutebrowser issue #8135](https://github.com/qutebrowser/qutebrowser/issues/8135), [sway wiki](https://github.com/swaywm/sway/wiki/Running-programs-natively-under-Wayland/39eac46a9e6c56ccb43ef7993fcc9871cb61c1b0), [sway wiki (older)](https://github.com/swaywm/sway/wiki/Running-programs-natively-under-Wayland/a540e1876ea875a6817ba7849e7fd4eba12f646f), [liri materialdecoration](https://github.com/liri-archive/materialdecoration), [XdgOutputManagerV1 QML](https://doc.qt.io/qt-6/qml-qtwayland-compositor-xdgshell-xdgoutputmanagerv1.html), [qwaylandscreen_p.h (ecloud mirror)](https://home.ecloud.org/doc/qt/da/dea/qwaylandscreen__p_8h_source.html), [Arch forum thread](https://bbs.archlinux.org/viewtopic.php?id=309603), [qtwayland README](https://github.com/qt/qtwayland/blob/dev/README), [Qt 4.8 QWaylandDisplay](https://dreamswork.github.io/qt4/structQWaylandDisplay.html), [qwaylandscreen_p.h (codebrowser)](https://codebrowser.dev/qt6/qtwayland/src/client/qwaylandscreen_p.h.html).
- systemd (official source): [logind.conf.xml](https://github.com/systemd/systemd/blob/main/man/logind.conf.xml), [org.freedesktop.login1.xml](https://github.com/systemd/systemd/blob/main/man/org.freedesktop.login1.xml), [Inhibitor locks](https://systemd.io/INHIBITOR_LOCKS/).
- libinput: [switches](https://wayland.freedesktop.org/libinput/doc/latest/switches.html). Kernel: [DRM KMS helpers](https://docs.kernel.org/gpu/drm-kms-helpers.html).
- Wayland protocol prose (mirror, not official): [xdg-output-unstable-v1](https://wayland.app/protocols/xdg-output-unstable-v1), [fractional-scale-v1](https://wayland.app/protocols/fractional-scale-v1), [wlr-layer-shell-unstable-v1](https://wayland.app/protocols/wlr-layer-shell-unstable-v1). Official freedesktop GitLab raw files returned 403 in this run.

---

**Recap.** I finished the read-only audit of the repository, the board, and the pinned upstream sources, then produced the complete board-ready plan above. Its core: keep KWin as live *and* restore authority (an upstream fact the wiki understates), add one focused `org.qindaqt.Display1` service as the single protocol writer with a service-owned confirmation timer and persistent revert journal, change the compositor plugin only additively (generation counter, virtual-hotplug test seam, Hybrid group reflow), keep the shell's geometry path untouched while fixing a suspected fractional-scale bijection defect, and keep Settings1 to policy/registry keys. Two upstream facts materially reshape the required test matrix (KWin rejects negative output positions; KWin persists previews immediately), one host fact creates a release question (Plasma's KScreen OSD is D-Bus-activatable here), and the only unverifiable item (Qt's `QScreen` derivation at fractional scale) is labelled as inference with a first-slice measurement. No file was edited and no command was run; next steps are the manager's verification of this run and assignment of slices D0/D1/D3b first, with the cross-lane questions in §12 routed to their owners.

