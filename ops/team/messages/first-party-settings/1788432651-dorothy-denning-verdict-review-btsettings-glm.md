# Independent exact-candidate review — Bluetooth Settings route

- Persona: **Dorothy Denning**, independent first-party route reviewer
- Provider/model: Z.AI GLM `zai-coding-plan/glm-5.3` (reasoning high)
- Exact candidate SHA: `bf7b00fec5a80f3d37795568d4dde6c35a72ea19` (verified `git rev-parse HEAD`)
- Candidate tree SHA: `b64c3c8ecaa101a5f0ef2d30acef8c5f5de3caed`
- Parent SHA: `ee187e97221ee7f13d6e4e00e6ee3356b6faf3d8`
- Base SHA: `ee187e97221ee7f13d6e4e00e6ee3356b6faf3d8` (parent is the base; single-commit candidate)
- Review worktree: `/home/cabewse/work_SPaC3/container-wm-workers/bluetooth-settings-route-glm-review` (detached at the candidate; `git status --porcelain` empty before and after review)
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-btsettings-glm`
- Implementer handoff: `ops/team/messages/first-party-settings/1788420262-grace-chisholm-young-handoff.md`

## Findings ledger

### P0

None.

### P1

**P1-1 — The Settings window becomes permanently unclosable after a discovery acquire completes without success during route departure.**

Route/model files:

- `src/apps/settings/bluetooth/bluetooth_settings_model.cpp:335-348` — `setRouteActive(false)` sets `m_releaseRequested = true` when an `AcquireDiscovery` is pending.
- `src/apps/settings/bluetooth/bluetooth_settings_model.cpp:350-391` — `handleOperationCompleted()`: when the awaited acquire completes with `Rejected`/`Failed`/`Uncertain` (or `!exact`), no lease is established and `m_releaseRequested` is never cleared (the flag-clearing at lines 385-389 applies only to `ReleaseDiscovery`).
- `src/apps/settings/bluetooth/bluetooth_settings_model.cpp:393-413` — `retireLeaseFromCurrentTruth()` returns early (`if (!m_discoveryLease) return;`) so it also never clears `m_releaseRequested` when no lease exists.
- `src/apps/settings/bluetooth/bluetooth_settings_model.cpp:441-456` — `tryAutomaticRelease()` returns early when `!m_discoveryLease`, never dispatching and never clearing the request.
- `src/apps/settings/bluetooth/bluetooth_settings_model.cpp:75-86` — `departureReleasePending()`'s third clause then reports a pending departure release while nothing is outstanding and no lease exists (`m_releaseRequested && !m_automaticReleaseBlocked && exactSnapshotReady()`).
- `src/apps/settings_center/Main.qml:38-55` — `onClosing` rejects every close while `bluetoothSettings.departureReleasePending` is true; the in-page Close button (`BluetoothPage.qml:175-184` → `root.close()`) and the platform Quit shortcut (`Main.qml:125-128` → `root.close()`) route through the same fence.
- `src/apps/settings_center/Main.qml:97-108` — the resolver re-closes only on `viewChanged` with `departureReleasePending == false`, which never arrives in this state.

Contract violated: `docs/wiki/apps/bluetooth-settings.md` ("Discovery lease lifetime") — "Window close waits while an admitted acquire/release is outstanding. If departure occurs while acquisition is pending, release follows only after that exact acquisition completes successfully", and the no-trap guarantee ("closing the process relies on Bluetooth1's caller-disappearance cleanup instead of trapping the window on an inadmissible request"). When the acquire completes without success there is no lease and nothing outstanding; the close must proceed. Instead the fence stays armed forever. All close paths are rejected: title bar, in-page Close, Ctrl+Q. The only escapes are (a) the service owner dying (makes `exactSnapshotReady()` false) or (b) navigating back to the Bluetooth tab — which clears the flags and then makes the still-armed `bluetoothClosePending` close the window from under the user on a mere route selection.

Reachability is ordinary, not hostile: `Rejected/too-many-leases` is a listed Bluetooth1 reason code (`docs/wiki/reference/bluetooth1-v1.md`), and the same stuck state is reached by a service-side `Failed` reply, a client operation timeout (`Uncertain`), or owner replacement mid-acquire (`synchronizeAuthority()` at `bluetooth_settings_model.cpp:415-423` resets `m_pending` but leaves `m_releaseRequested` set; the trap re-arms when the replacement owner's snapshot becomes ready).

Reproductions (scratch probes under the assigned build root, real candidate code, injected fake transport, no host bus):

1. Unit level — `/home/cabewse/work_SPaC3/builds/qindaqt/review-btsettings-glm/scratch/tst_departure_trap_probe.cpp`, built against the candidate Debug libraries:

   ```
   QT_QPA_PLATFORM=offscreen ./tst_departure_trap_probe
   → FAIL!  rejectedAcquireAfterDepartureTrapsWindowClose()
     '!pendingAfterFailure' returned FALSE.
     (departureReleasePending stuck true after a rejected acquire:
      the Settings window can never close)
   ```

   Observed: after the rejected acquire and a fresh valid snapshot (epoch 61, revision 8), `model.busy()` is false, `discoveryLeaseHeld()` is false, no release was ever submitted (`submissions == 1`), yet `departureReleasePending()` is true. Expected: false.

2. End-to-end through the real `Main.qml` — `/home/cabewse/work_SPaC3/builds/qindaqt/review-btsettings-glm/scratch/tst_window_close_trap_probe.cpp` (real `BluetoothSettingsModel`, real `Main.qml`, stub Customize like the project's own window-close test):

   ```
   QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
   DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ./tst_window_close_trap_probe
   → QWARN: review-probe: departureReleasePending after settle = 1
   → FAIL!  rejectedAcquireDuringCloseTrapsWindow()
     '!window->isVisible()' returned FALSE. (Settings window failed to close)
   ```

   Observed sequence: window close while acquire in flight → close correctly held; service replies `Rejected/too-many-leases` (valid wire result) → client refetch answered with valid snapshot → model idle, no lease → 300 ms later the window is still visible and a repeated `window->close()` is rejected again; no release was ever dispatched. Expected: the close completes once nothing is outstanding.

Why the candidate's own tests miss it: `tests/apps/settings/bluetooth/tst_bluetooth_settings_model.cpp:74-107` exercises only the successful acquire → convergence → departure-release path, and `tests/apps/settings/bluetooth/tst_bluetooth_window_close.cpp` stubs `departureReleasePending`, so the model's failure path never meets the QML fence.

Repair direction (for the implementer, same worktree): clear `m_releaseRequested` whenever the awaited acquire completes without success (no lease exists to release), and/or make `departureReleasePending()`'s third clause require that a lease actually exists or an admitted acquire/release is still outstanding. Add model + window-close coverage for rejected/uncertain acquire after departure.

### P2

None.

### P3

**P3-1 — Precision: `docs/wiki/development/testing-harness.md` ("Current Bluetooth Settings route proof") says the window-close row "requires an admitted discovery release to finish before the host exits".** The row (`tst_bluetooth_window_close.cpp`) stubs the model and toggles `departureReleasePending` manually; admission truth is not exercised in that row. The claim overstates what that row proves by one word. Nonblocking.

**P3-2 — No route-level hostile-snapshot negative control.** The review brief expects duplicate ids, overlong names, and out-of-range RSSI/class to fail closed. The model re-validates every snapshot (`exactSnapshotReady()` at `bluetooth_settings_model.cpp:88-97`), which is correct defense-in-depth, but no test feeds a malformed snapshot to the route model. The behavior is currently guaranteed by the protocol validator and the client's refusal to publish invalid snapshots (proven in `tests/services/bluetooth_*`), so this is a missing negative control for defense-in-depth, not a live defect. Nonblocking.

**P3-3 — Compact-host coverage observation.** The Settings Center compact-layout row stubs `bluetoothSettings` but never selects the Bluetooth route in the compact host (Bluetooth Escape/Tab/focus entry is asserted only in the wide host at `tst_settings_navigation_page.cpp:449-469`; the Bluetooth page's own compact scene is covered separately at `tst_bluetooth_page.cpp:145-161`). The wiki claims do not assert per-layout Bluetooth focus, so this is a coverage note only. Nonblocking.

## Review questions answered

1. **Authority split — pass.** No `Pair`/`Trust`/`Remove` path exists anywhere in `src/apps/settings/bluetooth/**` or its QML (grep leaves only `pairingSupported()` returning `false` and disclosure text). The boundary poison (`tests/apps/settings/bluetooth/check_boundary_negative.cmake:48-50`) proves a planted `Q_INVOKABLE bool pairDevice()` is rejected, and the include scan rejects private service headers with positive and negative controls (both rows pass). Connect/disconnect admission requires `paired` (`not-paired` otherwise, `bluetooth_settings_model.cpp:178-196`), and the model test proves an unpaired device's connect is rejected with zero submissions. Power and the single reference-counted lease go only through the public client's admission (`admissionReason`, `bluetooth_settings_model.cpp:142-199`), including capability, epoch, adapter-power, and `no-lease`/`lease-already-held` fences.
2. **Lease hygiene — pass on the tested paths, fail on one corner (P1-1).** Release on route departure works (`serializesDiscoveryAndReleasesOnDeparture`), window close waits for an admitted release (window-close row), owner loss mid-discovery retires the lease locally without sending to the new owner (`ownerReplacementClearsTruthAndRetiresLease`, and my probes confirm the fake-transport attack works as designed). The failed/uncertain acquire-after-departure corner is the P1 above: the lease-fence is left armed with no lease.
3. **Fencing and presentation — pass.** Displayed availability and dispatch share one predicate (`admissionReason` feeds both row maps at `bluetooth_settings_model.cpp:201-275` and `dispatch()` at `:277-307`). Stale replies fail the `exact` owner/epoch/revision/kind/initiating-revision check (`:355-359`); success keeps controls fenced until a matching authoritative snapshot revision arrives (convergence, `:369-383`, `:425-435`); replacement and malformed truth clear rows and close admission. Hostile snapshots cannot be displayed because `exactSnapshotReady()` re-validates (see P3-2 for the missing negative control). Loading/ready/degraded/unavailable/busy texts are truthful (`statusText`, error/operation labels), and the first-focus target is the always-enabled Close button (`BluetoothPage.qml:19,175-184`; `Button.qml` maps `available && !busy` to `enabled`; asserted in both page scenes and the wide navigation test).
4. **Settings Center integration — pass.** All shared edits are append-only: one `add_subdirectory` each in `src/CMakeLists.txt`/`tests/CMakeLists.txt`, one enum value at the end of `SettingsRouteComponent`, one registration after Customize, one host mapping, one Loader, one RPATH segment, one dependency row, one `Main.qml` import plus the Bluetooth-specific close-fence block, additive test extensions, additive wiki/nav rows. Registry order is notifications→appearance→display→network→customize→bluetooth with Ctrl+5 = Bluetooth, asserted together with PageTab role/selected, Escape, and Tab-into-Close under `QT_FATAL_WARNINGS=1` (`tst_settings_navigation_page.cpp:449-469`). The installed rows prove real relocation with private buses absent: withheld Bluetooth module forces exit 3 while the developer tree stays present, then the relocated route stays resident (both profiles).
5. **Boundaries and docs — pass with P3 notes.** The route itself contains no QtDBus include or service-internal include (allow-list scan plus negative controls, both profiles). `DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent` was set for every selector; nothing touched host buses, hardware, or network. `src/apps/settings_center/main.cpp` composes the public `QtBluetoothTransport`/`BluetoothClient` following the Network precedent and passes only the model into QML. Source shape passes (largest changed file 499 non-blank lines); no JSON changed. The wiki pages match the code except the lease-lifetime wording contradicted by P1-1 and the P3-1 precision note.

## Commands executed and results

All commands run from the review worktree at the exact candidate; scratch probes live only under `/home/cabewse/work_SPaC3/builds/qindaqt/review-btsettings-glm/scratch`. `<ROOT>` = `/home/cabewse/work_SPaC3/builds/qindaqt/review-btsettings-glm`.

| Command | Result |
| --- | --- |
| `git rev-parse HEAD` / `HEAD^{tree}` / `HEAD^` | `bf7b00fec5a80f3d37795568d4dde6c35a72ea19` / `b64c3c8e…` / `ee187e9722…` — matches the brief and handoff |
| `git status --porcelain` (before and after) | empty |
| Debug configure + focused build (`<ROOT>/debug`, exact recipe, targets `qindaqt_settings_bluetooth qindaqt_settings_bluetooth_qml qindaqt_bluetooth_settings_model_tests qindaqt_bluetooth_page_tests qindaqt_bluetooth_window_close_tests qindaqt-settings qindaqt_settings_route_registry_test qindaqt_settings_navigation_controller_test qindaqt_settings_navigation_page_test`) | exit 0, 523/523 steps, strict warnings on |
| Release configure + focused build (`<ROOT>/release`, same targets) | exit 0, 523/523 steps |
| `DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir <ROOT>/debug -R '^qindaqt\.settings-bluetooth-' --output-on-failure --no-tests=error` | exit 0, **6/6 passed** |
| Same selector, `<ROOT>/release` | exit 0, **6/6 passed** |
| `DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir <ROOT>/debug -R '^qindaqt\.(settings-(route-registry\|navigation-controller\|navigation-page)\|settings-app-(offscreen\|rejects-(unknown-route\|missing-theme)\|desktop-identity\|route-construction\|installed-routes))$' --output-on-failure --no-tests=error` | exit 0, **9/9 passed** |
| Same selector, `<ROOT>/release` | exit 0, **9/9 passed** |
| `./tools/validate-docs` | exit 0, 126 Markdown documents/navigation validated |
| `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir <ROOT>/site` | exit 0 |
| `./tools/check-source-shape` | exit 0 (largest changed shared test 499 non-blank lines) |
| `git diff --check` | exit 0 |
| `python3 -m json.tool` on changed JSON | not applicable — zero JSON files changed |
| Scratch probe `tst_departure_trap_probe` (unit P1 repro) | 1 failed / 2 passed — **reproduces P1-1** |
| Scratch probe `tst_window_close_trap_probe` (end-to-end P1 repro through real `Main.qml`) | 1 failed / 2 passed — **reproduces P1-1** |

No nested-compositor rows, host D-Bus services, hardware, uinput, or network were used at any point.

## Verdict

The candidate builds clean in both profiles, every owned selector and static gate is green, the authority split and fencing are genuinely enforced, and the integration edits are cleanly append-only. However, the window-close/departure fence traps the Settings window permanently after a legally rejected, failed, timed-out, or owner-interrupted discovery acquire during route departure — a contract violation of the route's own lease-lifetime guarantees, reproduced at both model level and through the real `Main.qml`.

VERDICT REJECT P0/P1/P2/P3=0/1/0/3
