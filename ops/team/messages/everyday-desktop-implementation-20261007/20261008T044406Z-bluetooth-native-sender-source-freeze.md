# Bluetooth native sender source freeze

- Timestamp: 2026-10-08T04:44:06Z
- Author: ed-foreign-astra-20261007
- Outcome: exact source review of the owned sender-preserving repair, not native or installed acceptance.
- Base: 5b7f5b5910e262a4bf2be2326bc92fc97af4e2ac; prior contract 88c3e8c685c00c8e0c55758f5fab8c707ee1ab8f.
- Resources: no compiler/private bus/runtime lease; no host/device/installed action.

## Source

The new private libdbus wire retains real sender and reply serial for driver,
helper, main-intent and BlueZ property replies, including errors. GUID-pinned
RadioServiceSession owns raw and Qt connections; full current owner-issued
request records authorityOwner and transportCaller. UID alone never delegates.
Private codec, helper service and authority are separate collaborators.
The globally bounded nonce ledger stays keyed by issuing authority owner.
Deferred completion rechecks admission and re-finds pending identity after
borrowed callbacks. No Qt internalPointer/private headers or broker policy
change. Invalid supplied addresses or a selected peer loss/mismatch fail closed.
Only pre-peer optional preparation failure retains Qt-only inventory with an
explicit no-write port. Main daemon sandbox and unrelated drop-ins are unchanged.

Own public factory/port install headers expose no native types. Module CMake
declares imported GLOBAL dbus-1 metadata; the installed static consumer links
that dependency and independently poisons/restores both exact staged headers.
Portage dependency closure still belongs to the manager/overlay package gate.
Settings window-close fixture adds only its existing PortalPermissions static
QML/plugin dependency, following the retained native missing-module failure.

## Authored native gates — UNRUN

The revised source adds actual private native service/port success with an
in-memory radio lease, wrong-current-address refusal, foreign correct-serial/
nonce replies, malformed/error/wrong-nonce/late/duplicate completion,
main/helper replacement, expiry, callback cancellation/destruction, explicit
raw delegation negatives, transport replacement without nonce-history reset,
same-path/new-broker captured-GUID rejection and selected-bus loss. The original
private authority and A/B/A ledger rows remain. No fixture accesses rfkill.

Exact prospective packet: .cache/bluetooth-native-logs/native-repair-acceptance-plan.json.
After review/grant only, from this worktree:
- python3 .cache/bluetooth-native-logs/run_native_repair.py build TAG
- python3 .cache/bluetooth-native-logs/run_native_repair.py main-tests TAG
- python3 .cache/bluetooth-native-logs/run_native_repair.py applet-configure TAG
- python3 .cache/bluetooth-native-logs/run_native_repair.py applet-build TAG
- python3 .cache/bluetooth-native-logs/run_native_repair.py applet-tests TAG

The original main selector is retained; three new source rows make32 prospective
rows, to verify against actual generated CTest registry after configure. The
standalone applet harness has7 expected rows. Radio staged SDK withhold/restore
is inside main selection. Saved targets-native-repair.json excludes applet
targets from the main build and adds only the three owning new test targets.
Old762 control uses run_old_control.py configure/build/run and immutable original
8c02 test assertions; preserve it separately from the new delegated-wire tests.
Do not label a new-interface fixture an unchanged old-source reproduction.

## Actual source-only validation

- tools/validate-docs: exit0,531 Markdown documents/navigation.
- mkdocs build --strict: exit0.
- owning radio static boundary: exit0.
- git diff --check: exit0.
- global source-shape: exit1,63 errors/169 total issues. Byte comparison of every
  reported path to exact base5b7 found zero changed files; no rule was relaxed.
- New radio production maximum233 nonblank lines at this checkpoint.
- Logs: .cache/bluetooth-source-final/; baseline comparison shapes-baseline.json.

Original814 strict compile0/main26-of29 exit8 and7d7 focused1-of3 exit8 remain
preserved. The Qt reply.service() defect and rejected638 broker premise are not
retroactively qualified. All current C++/fixture changes remain uncompiled.

Root's separate transient O_RDWR open/fstat/close transferred zero bytes and
proves access only under those test settings. Full effective helper-unit policy,
per-index mutation/readback, Portage adoption and ordinary installed controls
remain separate manager gates. ADR0359 remains Proposed.

Requested next action: root independent exact source review, then an explicitly
granted bounded native continuation. No program or ED05 outcome is closed.
