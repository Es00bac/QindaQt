# Exact-candidate review verdict — Desktop polish

- Reviewer persona: **Dorothy Hodgkin**, independent shell reviewer (`dorothy-hodgkin`)
- Provider/model: Moonshot Kimi `kimi-code/k3`
- Candidate SHA: `511ac8623051b78adfb54634d9a5fce0c466005f` (`worker/desktop-polish`, implementer Frances Arnold, OpenAI Codex)
- Candidate tree SHA: `142c77ba657c056f00655395789fee1ddbe4c5fd` (matches the handoff's declared tree)
- Parent SHA: `6a2019aa8dd6110054cca2ce0b4e8b23a9662251`
- Base SHA: `6a2019aa8dd6110054cca2ce0b4e8b23a9662251` (= main, identical to parent)
- Review worktree: `/home/cabewse/work_SPaC3/container-wm-workers/desktop-polish-k3-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-polish-k3`
- `git rev-parse HEAD` equaled the exact candidate SHA and `git status --porcelain` was empty before and after all work. No product path was edited; scratch output stayed under the build root.

## Review question findings

1. **Task icons.** `tests/session/DesktopVirtualAppletModules.cmake` stages the four first-party desktop entries into the DesktopVirtual component; `authenticate_first_party_desktop_entries` (tests/session/desktop_session_package_contract.py:94) fails the package-contract row on a missing entry or wrong `Icon=` identity (negative control: `test_first_party_desktop_entries_are_exact_and_missing_fails`, passed). Interactive/readiness validators reject a task button with empty `iconName`, `iconResolved != true`, or `applicationId == "qindaqt-shell"` (tests/session/desktop_session_shell_polish.py:41-52 and the mutation subtests in test_desktop_session_interactive_unit.py, which ran and passed inside `desktop.virtual.sandbox-unit`). The app-id → entry lookup itself (`DesktopEntryIconResolver::iconNameForAppId`) is unchanged by this lane; its case-insensitive/reverse-DNS-tail normalization is the documented I2 contract (docs/wiki/shell/iconography.md:73-76), and first-party ids match exactly. No new guessing introduced.
2. **Shell-owned / non-normal exclusion.** The contract now exists on both sides: the compositor publishes per-window `windowType` (`isNormalWindow()`) and `ownerRole` (PID equality with the authenticated `KWinShellPanelOwnerSource` panel owner, not uid — a legitimate application sharing the shell's uid keeps `Application`; no shell bound means no exclusions) at src/compositor/kwin/kwinshelltaskfacts.cpp:283-291, the codec makes both keys mandatory with hostile-string rejection (src/compositor/src/shelltaskfactscodec.cpp:155-235), and T0 filters `type != Normal || owner != Application` before output/workspace axes (src/shell/task_list/src/task_list_filter.cpp:8-15). Negative controls: `shellOwnedAndNonNormalWindowsAreNeverTasks` (tests/shell/task_list/tst_task_list_scope_filter.cpp:143-161, passed in both build types) proves a bound-shell normal window and another app's non-normal utility are excluded while a normal application window remains; `roleAndOwnerProvenanceRoundTripsAndRejectsHostileValues` (tests/compositor/tst_shelltaskfacts.cpp, passed) proves codec round-trip plus rejection of `ownerRole: "untrusted"`. Other apps' popups/panels are excluded as `NonNormal` regardless of owner, matching the documented rule.
3. **Quiet empty chips.** The white square was the trailing chip of the redundant legacy `grouped-task-list` alias in `data/profiles/qindaqt.json` (which also carries canonical `task-list`), visible in the base capture; the candidate drops the redundant alias via `RuntimePanelAppletCompatibility::normalize` (src/shell/runtime/runtimepanelappletcompatibility.cpp) and the idle status tray now has zero panel extent via token-styled gating in `StatusNotifierApplet.qml` plus the `emptyLiveContent` zero-extent guards in `PanelAppletRow.qml`/`PanelAppletColumn.qml`. Covered by the offscreen row `test_a_emptyTrayIsQuietAndHasNoAttentionMarker` (tests/shell/status_notifier/applet/qml/tst_StatusNotifierProductionPanelKeyboard.qml:169-183, passed) and the pure compatibility tests (qindaqt.shell-runtime-panel-applet-compatibility, passed). The launcher "attention dot" was the amber unresolved-plug-in marker on the `application-launcher` alias; its semantics are now documented in docs/wiki/shell/status-tray.md:136-140 ("reserved for an unresolved profile plug-in … not an idle-state or attention indicator"). Requirement satisfied.
4. **Do Not Disturb fresh profile.** Quieting now owns a purpose-scoped Settings1 client requesting only `services.doNotDisturb` (src/shell/runtime/shellruntimeapplication_applets.cpp:48-57, with an accurate AGENT-CONTRACT comment), so unrelated optional applet keys can no longer reject the whole subscription. Readiness drivers require `enabled=false, hasBaseline, state=ready, canToggle, empty status/error` before accepting the shell (desktop_session_shell_polish.py:59-68; `quieting-not-ready` is Pending and never accepted). Base vs candidate captures confirm the red "Do Not Disturb setting unavailable"/Retry is replaced by an available, off switch. DND/notification rows in the selector (notification-center, notification quieting bridge, readiness) all pass in both build types.
5. **Gate execution.** See "Commands and results" below; all executed, none inferred.

## Findings ledger

### P0 — none

### P1

**P1-1: The normative wire-contract reference was not updated for the schema change this commit makes.**

- Location: `docs/wiki/reference/compositor-control-v1.md:357-376` (schema example omits the now-mandatory `ownerRole` key) and `docs/wiki/reference/compositor-control-v1.md:383-384` ("`windowType` is currently exactly `normal` because the managed-window admission excludes other types").
- Reproduction: `grep -n "windowType\|ownerRole" docs/wiki/reference/compositor-control-v1.md` at the candidate returns only lines 364 and 383; there is no `ownerRole` anywhere in `docs/`. Meanwhile `src/compositor/src/shelltaskfactscodec.cpp:158-160` requires `ownerRole` in `exactKeys(...)` for every window object, and `src/compositor/kwin/kwinshelltaskfacts.cpp:283-287` emits `windowType: "non-normal"` for non-normal windows. Observed: the published reference contradicts the implemented schema. Expected per AGENTS.md ("Architecture in the wiki is normative … must update the documentation … in the same change"; "Code changes that leave relevant documentation … inaccurate are incomplete") and per this lane's owning-page list, which explicitly assigns the compositor reference: the reference page documents the changed payload, including `ownerRole` and the two-value `windowType` domain.
- Impact: a consumer implemented from the reference rejects every real post-change payload (fail-closed direction, so not destructive), and the stated enum domain is false. The four owning shell pages were updated; the normative protocol reference for the exact schema this lane changed was not. The task-list.md payload description was updated, which makes the stale reference a direct intra-wiki contradiction.

### P2 — none

### P3

**P3-1: Readiness validators equate per-row `buttons` with per-window `windowCount`.** `desktop_session_shell_polish.py:35` (`len(buttons) != windowCount`) and `desktopnotificationshellpolish.cpp:52-54` assume one presented row per visible window. A future scenario with a grouped container row (one row, `windowCount >= 2`) or more than 64 presented rows would be reported Invalid despite correct product behavior. Current scenarios are container-free and small, so no row is affected; noted for the next lane that adds containers to interactive evidence.

## Commands and results (all run from the review worktree unless noted)

| Command | Result |
| --- | --- |
| `git rev-parse HEAD`; `git status --porcelain` (before and after) | `511ac86…`; empty (clean) both times, exit 0 |
| `git diff --stat 6a2019aa..511ac86` | 62 files, +1383/−287 — reviewed in full |
| Debug configure (exact brief recipe, `-B <ROOT>/debug`) | exit 0 |
| Release configure (same, `-B <ROOT>/release`, `-DCMAKE_BUILD_TYPE=Release`) | exit 0 |
| `cmake --build <ROOT>/debug --parallel 3` (all 4258 targets, strict warnings on) | exit 0 |
| `cmake --build <ROOT>/release --parallel 3` (all 4258 targets) | exit 0 |
| `env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent TMPDIR=<ROOT>/debug/tmp ctest --test-dir <ROOT>/debug -R '^qindaqt\.(shell-\|applet\|task-list-\|status-notifier-\|launcher\|notification-center\|compositor)\|^desktop\.virtual\.(sandbox-unit\|package-contract\|stage-closure\|notification-shell-readiness-unit)' --output-on-failure --no-tests=error` | exit 0, **114/114 passed** (log: `<ROOT>/ctest-debug-main.log`); includes the new `qindaqt.shell-runtime-panel-applet-compatibility`, `qindaqt.task-list-scope-filter`, `qindaqt.status-notifier-applet-production-panel-keyboard-offscreen`, and all four safe DesktopVirtual rows |
| Same environment, `-R '^compositor\.shell-task-facts$'` (Debug) | exit 0, **1/1 passed** |
| Same two selectors against `<ROOT>/release` | main: exit 0, **114/114 passed** (log `<ROOT>/ctest-release-main.log`); facts: exit 0, **1/1 passed** |
| `./tools/validate-docs` | exit 0, 147 Markdown documents and navigation validated |
| `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir <ROOT>/site` | exit 0 |
| `./tools/check-source-shape` | exit 0, 2,598 files; warnings are pre-existing out-of-lane review-threshold files, none in the changed set |
| `git diff --check 6a2019aa..511ac86` | exit 0 |
| `python3 -m json.tool` on both changed probe fixtures | exit 0 each |
| `cd tests/session && python3 -m unittest test_desktop_session_interactive_unit` | 6 tests, OK (mutation negative controls execute) |
| Handoff capture comparison (`after-1080p.png` vs `capture-main-6a2019aa.png`, visual inspection) | corroborates: two resolved task icons, no `qindaqt-shell` task, no trailing white chip, no launcher amber dot, DND available and off, no red unavailable message |

Nested-compositor rows, host D-Bus services, hardware, uinput, and network were not run, per the review contract; the implementer's nested-row evidence (6/6 in both build types) was not independently re-executed.

## Verdict

All functional claims verify: builds are clean under strict warnings in both build types, all 115 selected rows pass in Debug and Release, static and JSON gates pass, the four review questions resolve in the candidate's favor, and the captures corroborate the outcome. One blocking documentation defect remains: the normative compositor protocol reference still documents the pre-change TaskListSnapshot schema (P1-1). The repair is confined to `docs/wiki/reference/compositor-control-v1.md` and requires no product-code change; the reviewer should recheck the repaired commit.

VERDICT REJECT P0/P1/P2/P3=0/1/0/1
