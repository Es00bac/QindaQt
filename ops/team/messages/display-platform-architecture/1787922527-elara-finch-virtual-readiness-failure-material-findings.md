# Elara Finch — virtual desktop S0+S1 readiness failure: material findings (midpoint)

- Timestamp: 2026-08-28T13:08:47Z
- Analyst: Elara Finch (Anthropic Claude Fable 5, `claude-fable-5`; analysis/exact review only)
- Analyzed immutable identity: HEAD `3320afdb4afad1c396b85add576f60d59e1d3b57`, tree `b5664f1e65a3d3984d88157c8083533956fa0462`, parent `e2ab439c79277464ebd9a9a8cba7d44b502cf17e`; read-only worktree `virtual-readiness-review-elara` (clean).
- Analyzed run: `ea96a7ab461ac31584da1174853368f7` in `build/virtual-desktop-private-1787919703/tests/session/desktop-session-results/` (`result.json` md5 `25ccb051…`, `sandbox.log` md5 `e0687811…`, 60 logs, 52 zero-byte `session-probe-NNN.log`).
- Also read, **not reviewed**: Rhea's descendant `e325f2f1e8d69d2d6e3eaa42c04df0f71d2265c7` (diff via the shared object store) and its sibling archive `26e772f23f519434ce445dca4ff51128` (Rhea `1787921728`), whose 51 archived stdout snapshots are the ground truth used in F2.
- Method: read-only Git/source/archive inspection plus one in-memory replay of the pure validator function `_snapshot_pending` (3320afdb `desktop_session_runtime.py:75-140`) against the archived snapshots with `PYTHONDONTWRITEBYTECODE=1`. No product edit, Git mutation, build, CTest, session, compositor, bus, UI, display/input endpoint, or host-state action.

All line anchors below are at `3320afdb` unless stated.

## F1 (P1, harness defect) — exact mechanism of run `ea96a7ab`

Timeline (UTC): sandbox start 12:44:51.842 (`startedUnixNs`); probe-001 log created 12:44:52.070; probe-052 at 12:45:06.944; readiness deadline ≈ 12:45:07.07; sandbox exit 12:45:07.108 (`finishedUnixNs`). 52 attempts, mean spacing ≈ 0.286 s, i.e. the cadence is the probe's own 250 ms post-marker sleep plus start-up.

- `tests/session/desktop_session_runtime.py:299-309` `sample()`: line 303 `current.wait(timeout=min(1.0, remaining))` waits for the **previous** probe to exit using the **outer** readiness remainder. The previous probe is still inside `QThread::msleep(250)` (`tests/session/desktopsessionprobe.cpp:149-152`), so once `remaining` < 0.25 s `Popen.wait` raises `subprocess.TimeoutExpired` — here after 0.0718 s.
- `subprocess.TimeoutExpired` is not a `RuntimeError`; `await_complete_snapshot` (`:143-165`) does not catch it, so `last_pending` is dropped. `test_desktop_session_nested.py:316-328` catches `subprocess.SubprocessError` generically and prints `Command '[...probe]' timed out after 0.0718…` — the archived `sandbox.log`.
- Two more deadline-edge exits also discard the reason: `:305` (`"deadline expired"`) and `:284` (`"timed out waiting for the probe"`). Only `:157` and `:160` keep it.
- `:280-288` consumes each probe's stdout line without archiving it; `:264-271` logs only stderr — hence 52 empty attempt logs.
- Teardown did run and was authenticated: `run_inner` `finally` (`:395-396`) executed `_cleanup`; had it raised, `main()` would have printed the `ProcessContractError` text instead of the `TimeoutExpired` text. Its terminal-phase ledger is nevertheless discarded on the failure path (`cleanup_records` is only consumed on success, `:399-403`). The kernel facts stand: bwrap exited 12:45:07.108, so the PID namespace collapsed within ~40 ms of the exception; `timedOut=false`, the 55 s outer deadline never engaged.

## F2 (three P1s) — readiness could never have passed on this tree; the 72 ms edge is a symptom

Replaying `_snapshot_pending` over the 51 real snapshots of run `26e772f2` gives, in validator stop order:

1. **Settings application ID — P1 product defect (Settings application owner).** `desktop_session_topology.py:100` expects `org.qindaqt.Settings`; `src/apps/settings_center/main.cpp:14-16` sets only `setApplicationName("qindaqt-settings")`/`setOrganizationName` and never calls `setDesktopFileName`, although `src/apps/settings_center/CMakeLists.txt:15` installs `org.qindaqt.Settings.desktop` and the editor does it correctly (`src/apps/text_editor/main.cpp:68`). Qt Wayland's xdg `app_id` falls back to the executable basename, and the compositor publishes `resourceClass()` (`src/compositor/kwin/managedwindowregistry.cpp:318`), so every snapshot carries `applicationId: "qindaqt-settings"`.
2. **Output name — P1 harness+doc defect.** `desktop_session_topology.py:74,108,249` and fixtures `test_desktop_session_topology_unit.py:54,71,72` assert `Virtual-1`; KWin 6.6.5's virtual backend names the first output `Virtual-0`, which the harness itself writes as `connectorName` (`tests/session/nested_session_scenario.py:191`) and which `Outputs`, `ShellVisibilitySnapshot` (`outputs[0].id`) and both dock records report. ADR-0026:74 and `testing-harness.md:969,971` codify the wrong name. The `tests/scenarios/*.json` `Virtual-1` names are declared-but-unapplied by documentation.
3. **Input-device predicate — P1 harness defect, not in Rhea's `1787921728` list.** `desktop_session_topology.py:270-277` requires `keyboard is True and pointer is True` booleans on a device record. The real producer emits `capabilities: ["keyboard","pointer"]` and no such booleans (`src/compositor/kwin/inputcapabilities.cpp:56-57,90-91,102-124`); the positive fixture (`test_desktop_session_topology_unit.py:60-66`) invented them. With (1) and (2) corrected the archived snapshot still returns `exactly one combined development input is required`.

With all three corrected, attempt **2** (+0.27 s after probe-001, ≈0.85 s after bwrap start) and all 49 later attempts satisfy the predicate: the contained desktop — compositor, private bus, Settings1, Audio1, Notifications, two mapped/committed `dock` surfaces owned by shell PID 76, Text Editor `org.qindaqt.TextEditor`, Settings mapped with title `QindaQt Settings — Notifications` — was fully ready for ~14.9 s of the 15 s window. Product boot latency is not the problem; the 15 s cap has >50× margin on this host.

Verified passing on the way: probe/argv signatures (`ay` methods, `compositor/dbus/org.qindaqt.Compositor1.xml`), generations as canonical decimal strings (`kwinoutputinventory.cpp:224-225`), dock `processId` as decimal string (`kwincontrolendpoint.cpp:264-265`), parentage (`src/session/main.cpp:37` execvp, `kwincommandbuilder.cpp:99` `--exit-with-session`, supervisor `QProcess` children) — so `_validate_processes` should bind `session→compositor` and `notification/shell→session` as the topology expects.

## F3 (P1 anti-repair) — do **not** adopt `qindaqt-settings` as the expected identity

Rhea's `1787921852` claim proposes making the observed `qindaqt-settings` "the expected truth". I recommend against it: (a) it enshrines Qt's fallback (`QWaylandWindow::initWindow`: desktopFileName, else executable baseName prefixed by a reversed `organizationDomain` when one is set — adding `setOrganizationDomain("qindaqt.org")` to the Settings app, as shell/services already do, would silently change the ID to `org.qindaqt.qindaqt-settings` and break the relaxed harness); (b) it contradicts the installed desktop entry ID that task identity, the `desktop-entry` notification hint, and launch routing rely on; (c) it converts S1 evidence into an assertion of a product defect. The smallest safe repair is one product line, `application.setDesktopFileName(QStringLiteral("org.qindaqt.Settings"));` after `settings_center/main.cpp:16`, mirroring the editor. The harness should keep `org.qindaqt.Settings` and stay truthfully red until the Settings owner (or the manager, by explicit ownership grant to Rhea) lands that line in the reviewed descendant.

## F4 — gaps in the proposed bounded repair (design as described in `1787921538`/`1787921728` and the `e325f2f1` diff; not an exact review)

- **D1 (P1)**: a probe lifetime expiring **before its first marker** is terminal (`desktop_session_readiness.py` `read_probe_document` → `ReadinessDeadlineExpired` → `await_complete_snapshot` raises), while the probe still blocks internally up to 15 s for service ownership (`desktopsessionprobe.cpp:103-107`). On a loaded host a >1 s Notifications start becomes a hard failure with `last_pending = "no snapshot was sampled"`. A lifetime expiry must be a retryable pending observation bounded by the 15 s cap, and/or the probe must become a pure sampler (no internal wait).
- **D2 (P2)**: the 250 ms sleep is a timed grace, not a handshake; on the success path the final probe's identity is captured only after `_build_evidence`'s /proc scan (`desktop_session_runtime.py:383-384`), racing that sleep under load. Capture the accepted probe's identity immediately after its marker, or hold the probe on stdin until released.
- **D3 (P2)**: no failure-path artifact — attempt summary, per-attempt timings, last pending, exception text, and the cleanup ledger are all lost on failure; only stdout lines are now archived.
- **D4 (P2)**: the three descendant units do not pin the exact regression (previous probe still alive with <lifetime outer remainder must not raise `TimeoutExpired`), a marker-less probe, a marker-then-hang probe, a `Popen`-faked `_await_runtime_snapshot`, or the killed-probe entry in the ledger.
- **D5 (P3)**: the probe never sets a D-Bus call timeout (QtDBus default 25 s), and non-`ReadinessDeadlineExpired` exceptions from `sample` still lose `last_pending`.

## F5 (P1 process) — fixture provenance

All three F2 mismatches passed 43–48 green units and two exact source reviews because `ready_probe()`/`valid_evidence()` were invented rather than derived from the compositor's producers. Acceptance must include one real archived probe snapshot as a committed fixture consumed by the positive readiness unit, plus a producer-shape check.

The complete P0–P3 handoff with the smallest safe repair, executable acceptance design, and routing follows in my next reply.
