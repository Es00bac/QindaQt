# Bluetooth selected-radio source draft — native qualification pending

- Author: ed-foreign-astra-20261007; requested Astra routing, no invented runtime attestation.
- Worktree/branch: everyday-bluetooth-recovery-20261008 / worker/everyday-bluetooth-recovery-20261008.
- Exact base: 5b7f5b5910e262a4bf2be2326bc92fc97af4e2ac.
- Source freeze only. No compilation, private-bus execution, namespace/device open, radio write, installation or user-repair completion is claimed.

## Authored boundary

Optional Portage-owned BluetoothRadio1 helper/client, separate policy/IPC/Linux collaborators, exact complete-request Current intent check, same-session Bluetooth1 owner/current caller and exact BlueZ owner/Address admission, bounded nonce/deadline policy, pinned SYSFS HCI-to-rfkill parent join, one selected CHANGE and current readback. Linux authority rechecks inside the opened writer path. No root/KAuth, CHANGE_ALL, startup/default write, replay, reblock, scan or pairing.

The helper's user unit uses the existing active-user ACL; direct D-Bus Exec fallback is disabled. Actual RW namespace/open qualification is still required. The main Bluetooth daemon unit and unrelated drop-ins are untouched.

Backend/service composition now delegates explicit enable, preserves direct legacy behavior only for definitive no-write absence/unobservability, and fences queued/helper/BlueZ work on caller/run/owner/object retirement. Current Powered comes only from properties; Failed is uncertain power outcome, not synthetic RFKill or no-effect truth. Cohesive Settings failure-text extraction and public-reason applet messages preserve the schema/API.

## Authored tests and current evidence

New radio operation, complete-intent, authority and private-BlueZ power fixtures cover replay/expiry, block/refusal, loss before/after possible write, exact issued arguments, caller/helper replacement, delayed same-address replacement and Failed versus later property truth. Staged public SDK consumer and exact-header withhold/restore gate are prepared. UI tests use public Settings/client and applet paths. None has run yet.

Actual source-only gates: documentation validation 531 documents/navigation exit 0; strict MkDocs exit 0 (8.24 seconds before the final owning harness paragraph); BlueZ module boundary exit 0; Settings boundary 10 files exit 0; applet boundary 5 files/four poison rejections exit 0; radio sandbox/public boundary exit 0; diff check exit 0. Changed C++/header source maximum is 495 nonblank lines after Settings decomposition. Full final docs/static rerun is recorded with the source freeze.

## Requested next gate

Manager supplies controlled new integration base if required, then different-author source review and sole focused native lease. Build only owning helper/service, new radio operation/intent/authority and BlueZ power targets plus existing affected Bluetooth/Settings/applet rows and staged SDK. Keep -j24 -l24 and private fixture isolation. Preserve exact failures and repair descendants without weakening validators or sandbox.

The actual Linux helper syscall path still needs separately authorized namespace/device qualification; synthetic policy/IPC tests cannot establish physical effects. Root owns all live radio actions. Clipboard final independent evidence ACCEPT is separately preserved at 95dba92a; ED05 source remains preserved and required after critical core repair.
