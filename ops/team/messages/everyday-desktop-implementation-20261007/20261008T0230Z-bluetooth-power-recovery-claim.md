# Selected-adapter Bluetooth recovery implementation claim

- Author: ed-foreign-astra-20261007
- Updated: 2026-10-08T02:32:24Z
- Exact base: 5b7f5b5910e262a4bf2be2326bc92fc97af4e2ac; fetched qinda local hub before isolated worktree creation.
- Branch/worktree: worker/everyday-bluetooth-recovery-20261008 / everyday-bluetooth-recovery-20261008.
- Manager-approved outcome: permanent explicitly requested selected-adapter radio recovery and truthful power diagnostics; no completion claim from source.

## Owned paths and boundaries

Existing src/services/bluetooth_bluez_adapter public constructor/private collaborators and CMake; src/services/bluetooth_service/app/main.cpp and owning CMake; new src/services/bluetooth_radio_helper public bounded contract/port, private authority/sysfs/rfkill/IPC collaborators, application, unit and activation data; focused tests/services/bluetooth_radio_helper and existing Bluetooth backend/service fixtures. Settings Bluetooth model failure text and shell bluetooth_request_state.cpp consume only fixed public reason codes. ADR0359, owning Bluetooth wiki/reference/module-boundary and minimal navigation edits. Minimal additive src/CMakeLists.txt and tests/CMakeLists.txt registry rows flagged to manager before edits.

Main Bluetooth daemon PrivateDevices/NoNewPrivileges and unrelated installed drop-ins are preserved. All installed artifacts must travel in the owning gui-wm/qindaqt-desktop Portage payload. No new privileged service, KAuth/URfkill dependency, ambient helper spawning, global radio/default writes, startup unblock, reblock, restart replay, scan or pairing action.

## Evidence limits and initial sequence

Root observed actual selected software unblock succeed, then immediate power error Failed, then later validated Powered=true and user speaker connection. Failed is not evidence of no effect and does not identify RFKill. Root read-only user helper namespace probe verified device read-open, not write access. RW qualification remains a manager-owned gate.

Proposed explicit enable sequence observes/unblocks only the proven selected radio, then submits one BlueZ power intent with current owner/target/caller checks. A missing helper or definitely no-write observation unavailability preserves the normal direct BlueZ authority path; known blocks and possible helper writes produce typed refusal/uncertainty. No automatic retry of a failed BlueZ intent. Actual later snapshot truth is distinct from the request receipt.

Source-first diagnostics/contract then helper/backend collaborator implementation and direct old/fixed/private fixtures. No compiler, bus/display, host radio/clipboard or installed actions by this worker. Clipboard repaired exact recheck preempts this source work when available.
