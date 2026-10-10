# Laptop keyring fix installed

Exact accepted product source: `a9c9768f46d05b3512ab4dd32e267412e994a083` over `c05721d48388ce36512172ff6886f9ebd0ffea42`. Exact accepted overlay: `e3b515d20661f9fd76ffd0159b840d54fc67235b`. Both are preserved in qinda's `fix/keyring-prompts-20261009` hub branches.

Changed product paths: `src/services/keyring/daemon/prompts.cpp`, `secret_service.h`, `src/services/keyring/prompt/Main.qml`, the two-line strict-build signal read in `src/services/keyring/app/main.cpp`, focused keyring fixtures, and `docs/wiki/architecture/keyring-daemon.md`. No persistence format, password transport, PAM authority or provider change.

Evidence:

- Strict Release daemon/prompt/focus target build: exit 0.
- Shared unlock, owner cancel/disconnect, matching collection cases: exit 0, 3/3, 12.656 seconds.
- Offscreen QML CTest: exit 0, 1/1; 7 Qt pass, 0 fail/skip, 0.47 seconds.
- Installed-binary private three-client smoke: exit 0, 1/1, 4.338 seconds; one helper and all callers complete.
- Wiki links/navigation: exit 0, 551 pages; strict MkDocs: exit 0.
- Normal signed Portage R28 build and installation: exit 0 each. Full GPG signature, embedded recipe and 2,622-path parity pass; exactly `usr/bin/qindaqt-keyring` and `usr/bin/qindaqt-keyring-prompt` differ from R25. Both installed SHA256 values match that signed image.
- Live Settings1 and native policy readback confirm the owner's explicitly approved `lockOnScreenLock=false`, `lockAfterIdleMinutes=0`; login remains unlocked. Original PID 3777976 is preserved, so the new daemon queue code adopts at next login. No live passwords, stored item records or actual password prompts were used.

Signed gpkg SHA256: `cc5fd4e7ea7a4e6bca40eeb0da7b12767997728509a860c32be6a8b793dc436f`. Artifacts and verification JSON are preserved on qinda under the ignored `.qa/keyring-prompts-20261009` staging directory. Rejected/unreleased R26/R27 recipes are masked; staged failures remain in history. Native physical focus is not independently qualified by the offscreen test.

Next action: integrate the exact accepted source and recipe without changing their tested code. The current user session already has the approved relocking correction. Available for a concrete regression recheck; no broader backlog or release completion is claimed.
