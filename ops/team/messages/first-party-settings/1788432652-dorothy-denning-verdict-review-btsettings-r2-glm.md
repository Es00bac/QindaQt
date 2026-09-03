# Independent exact-candidate recheck — Bluetooth Settings close-liveness repair

- Persona: **Dorothy Denning**, independent first-party route reviewer (same-reviewer recheck of the repair descendant per workflow)
- Provider/model: Z.AI GLM `zai-coding-plan/glm-5.3` (reasoning high)
- Exact candidate SHA: `24129a26e5d7c6bb01e1dd9e287c75a8db1c224d` (verified `git rev-parse HEAD`)
- Candidate tree SHA: `4e66338ba028793132bff93e3781e5cd430035ee`
- Parent SHA: `d7a53f8ed09d75956586d29fdbb264e36c1da3d4` (handoff-recording commit; no product paths)
- Product ancestor / prior rejected candidate: `bf7b00fec5a80f3d37795568d4dde6c35a72ea19` (rejected 0/1/0/3 in `lanes/review-btsettings-glm/verdict.md`)
- Lane base SHA: `ee187e97221ee7f13d6e4e00e6ee3356b6faf3d8`
- Review worktree: `/home/cabewse/work_SPaC3/container-wm-workers/bluetooth-settings-route-glm-review` (detached at the candidate; `git status --porcelain` empty before and after review)
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-btsettings-glm` (rebuilt incrementally; probes under `scratch/`)
- Implementer repair handoff: `last-message.md` and `ops/team/messages/first-party-settings/1788422550-grace-chisholm-young-repair-handoff.md`

## Repair understood before probing

`bf7b00f..24129a2` product paths only: the model cpp/h, the Bluetooth test CMakeLists (append-only), two test files, and the two owning wiki pages. The fix has three cooperating guards:

1. `bluetooth_settings_model.cpp:84-86` — `departureReleasePending()`'s wait-for-release clause now also requires `m_discoveryLease.has_value()`, so the fence can only stay armed for a real held lease (or, via the first clause, an outstanding admitted acquire/release).
2. `bluetooth_settings_model.cpp:391-398` — when an awaited `AcquireDiscovery` completes and no lease exists (rejected/failed/uncertain/inexact/malformed), the departure request and blocked flag are cleared. The lease-held success path is untouched: the lease is set first, so the guard cannot fire.
3. `bluetooth_settings_model.cpp:428-437` — when `synchronizeAuthority()` retires an in-flight acquire on owner loss/replacement and no lease exists, the same flags clear, which also prevents the trap from re-arming once the replacement owner's snapshot becomes ready (the re-arm corner from my prior verdict).

Guard-2 cannot clear a blocked automatic release that matters: `m_automaticReleaseBlocked` is only set on `ReleaseDiscovery` completions or release-dispatch failure, both excluded by the acquire-kind condition. A pending acquire also implies no lease at dispatch (`lease-already-held` admission), so `acquire completed && !lease` identifies exactly "acquire established nothing to release".

## Findings ledger

### P0

None.

### P1

None. **P1-1 is closed** — reproduced fixed at both levels with my original probes, rerun unchanged, plus new variants:

1. Original model-level probe `scratch/tst_departure_trap_probe` (rejected acquire after departure; failed at `bf7b00f`): **3 passed, 0 failed** — `departureReleasePending()` false after the rejected reply and stays false after fresh valid truth; no release ever submitted (`submissions == 1`).
2. Original end-to-end probe `scratch/tst_window_close_trap_probe` (real `BluetoothSettingsModel`, real Settings `Main.qml`, stub Customize; failed at `bf7b00f`): **3 passed, 0 failed** — `review-probe: departureReleasePending after settle = 0` (was 1), the held window close completes, repeated closes succeed.
3. New r2 model probe `scratch/tst_departure_trap_probe_r2`: **8 passed, 0 failed** — fence clears for rejected, failed, and uncertain acquires after departure; clears on owner-loss and owner-replacement and does **not** re-arm after the replacement owner publishes fresh valid truth; and the held-lease control proves the successful acquire still establishes the lease, serializes the departure release behind convergence, dispatches it, and reports pending until that exact release completes (over-fix ruled out).
4. New r2 QML probe `scratch/tst_window_close_trap_probe_r2` (real `Main.qml`): **5 passed, 0 failed** — close during a pending acquire completes for uncertain, owner-loss, and owner-replacement outcomes; no release ever sent; replacement-owner truth does not re-arm the fence.

### P2

None.

### P3

None new. The three prior P3s are closed with real coverage, not prose:

- **P3-1 closed** — `testing-harness.md` now says the window-close row "uses a stub to isolate successful-release waiting, then uses the real client/model to prove rejected, uncertain, owner-lost, and owner-replaced pending acquires release the QML close fence", which is exactly what `tst_bluetooth_window_close.cpp` does (`waitsForDiscoveryReleaseBeforeClosing` stub row unchanged from the ancestor; new real-model rows beside it).
- **P3-2 closed** — `tst_bluetooth_settings_adversarial.cpp::malformedSnapshotNeverBecomesActionable` feeds duplicate ids, a 257-char name, RSSI −129, and class 999 through the fake transport: client Unavailable/`malformed-snapshot`, no snapshot, model not ready, empty rows, and `requestDiscovery` refused with zero submissions. A route-level fail-closed negative control now exists.
- **P3-3 closed** — `tst_bluetooth_window_close.cpp::compactHostProvidesBluetoothFocusPath` drives the compact Settings host (440×360, `isCompact`), asserts the Bluetooth PageTab role/selected state, Escape returns focus to the tab, Tab lands on the always-enabled `bluetoothCloseButton` (objectNames verified present in `SettingsCompactHeader.qml:63` and `BluetoothPage.qml:177`).

## Review questions answered

1. **P1-1 closed — yes, with direct probe evidence at both levels** (see ledger). Both original probes pass unchanged; uncertain and owner-loss variants pass at model level and through the real `Main.qml`; the lease-held path still waits for its serialized release (guard cannot fire when a lease exists). The variants are registered in-tree: `qindaqt.settings-bluetooth-model-adversarial` (rejected/failed/uncertain/inexact/owner-loss/owner-replacement + malformed snapshots) and the extended `qindaqt.settings-bluetooth-window-close` (rejected/uncertain/owner-loss/owner-replacement through the real QML fence, plus compact-host focus). Neither new test is vacuous: both drive the real client/model (and real `Main.qml` for the QML rows) through the injected fake transport.
2. **P3s closed — all three, truthfully** (see ledger). The updated wiki lease-lifetime sentence ("A rejected, failed, uncertain, malformed, or owner-interrupted acquire establishes no lease, clears the departure wait, and cannot strand the Settings window") matches the code: each named outcome lands in the `!exact`/non-success branch or the authority-retirement branch, both of which clear the flags when no lease exists.
3. **Selectors and gates — all green; scope bounded; registries append-only.** Bluetooth selector 7/7 and Settings Center selector 9/9 in Debug and Release with `DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent`; all four static gates pass; the only build-registry edit is the append-only Bluetooth test CMakeLists block; `AGENTS.md`, `module-boundaries.md`, `coding-practices.md`, `bluetooth1-v1.md`, `src/CMakeLists.txt`, `tests/CMakeLists.txt`, and all settings-center sources are byte-identical to the ancestor I previously reviewed (empty `git diff bf7b00f..24129a2` on those paths). The prior round's integration findings (append-only Settings Center edits, authority split, boundary poison, installed relocation) are unaffected by this repair.

## Commands executed and results

All commands run from the review worktree at the exact candidate; probes live only under `/home/cabewse/work_SPaC3/builds/qindaqt/review-btsettings-glm/scratch`. `<ROOT>` = `/home/cabewse/work_SPaC3/builds/qindaqt/review-btsettings-glm`.

| Command | Result |
| --- | --- |
| `git rev-parse HEAD` / `HEAD^{tree}` / `HEAD^` | `24129a26e5d7c6bb01e1dd9e287c75a8db1c224d` / `4e66338ba0…` / `d7a53f8ed0…` — matches the lane and repair handoff |
| `git status --porcelain` (before and after) | empty |
| Debug reconfigure (exact lane recipe, `<ROOT>/debug`) | exit 0 |
| Focused Debug build, targets incl. new `qindaqt_bluetooth_settings_adversarial_tests` | exit 0, 42/42 steps, strict warnings on |
| Release reconfigure + focused build (same targets, `<ROOT>/release`) | exit 0, 42/42 steps |
| `DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir <ROOT>/debug -R '^qindaqt\.settings-bluetooth-' --output-on-failure --no-tests=error` | exit 0, **7/7 passed** (model, model-adversarial, page, window-close, boundary, boundary-poison, installed-route) |
| Same selector, `<ROOT>/release` | exit 0, **7/7 passed** |
| Settings Center selector (route-registry, navigation-controller, navigation-page, settings-app offscreen/rejects-*/desktop-identity/route-construction/installed-routes), `<ROOT>/debug` | exit 0, **9/9 passed** |
| Same selector, `<ROOT>/release` | exit 0, **9/9 passed** |
| `./tools/validate-docs` | exit 0, 126 Markdown documents and navigation validated |
| `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir <ROOT>/site` | exit 0 |
| `./tools/check-source-shape` | exit 0 |
| `git diff --check` | exit 0 |
| `python3 -m json.tool` on changed JSON | not applicable — zero JSON files changed |
| `scratch/tst_departure_trap_probe` (original P1-1 unit repro, rerun unchanged) | 3 passed / 0 failed — **fixed** |
| `scratch/tst_window_close_trap_probe` (original P1-1 `Main.qml` repro, rerun unchanged) | 3 passed / 0 failed — **fixed** |
| `scratch/tst_departure_trap_probe_r2` (rejected/failed/uncertain/owner-loss/owner-replacement + held-lease control) | 8 passed / 0 failed |
| `scratch/tst_window_close_trap_probe_r2` (uncertain/owner-loss/owner-replacement through real `Main.qml`) | 5 passed / 0 failed |

Probes were rebuilt against the candidate Debug libraries via `scratch/build_probes.py` (moc + compile + link reusing the exact test-target flags; no host bus, offscreen/software where QML is involved). No nested-compositor rows, host D-Bus services, hardware, uinput, or network were used at any point.

## Verdict

The repair is minimal, correct, and placed at the right boundaries: the departure fence now tracks only a real held lease or an outstanding admitted operation, every unsuccessful-acquire shape clears it, and the held-lease serialization path is provably unchanged. My original probes pass unchanged, the new variant coverage is genuine (real client/model/QML, no stubs on the behavior under test), the prior P3s are closed with real negative controls, documentation now claims exactly what the code proves, and every selector and static gate is green in both profiles with scope strictly bounded and append-only.

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/0
