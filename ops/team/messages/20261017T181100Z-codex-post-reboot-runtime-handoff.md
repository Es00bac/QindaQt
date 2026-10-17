# Post-reboot runtime recovery and boot audit handoff

Updated 2026-10-17T18:11:08+00:00. Source repair `37988eb8f`, recipe/source continuation
`84bb6d4ca`, reviewed provider `be99a1b`, Gabbee recipe `77dec16`,
overlay `8dfc954`. Independent source/provider/package verdicts are integrated
under `ops/team/messages/post-reboot-radio-review/`.

## Delivered runtime outcome

- The selected HCI unblock restored Powered=true; a controller connected.
- The actual installed helper now uses the reviewed sandbox setting through
  its scoped per-user override. Active/running, Result=success, name owned.
- Public Bluetooth1 SetPowered(true) returns Succeeded/adapter-power-set and
  preserves the one connected controller; no radio-off test was performed.
- Media Next is saved and both Gabbee actions are active in Shortcuts1.
- Gabbee r8 Portage install exits0; 262 object hashes match; the installed
  watcher is byte-identical to reviewed source. Both current Session endpoints
  verify, and the watchdog exits0. Both failed-unit lists are empty.

## Acceptance evidence and limits

Integrated radio CTests7/7, provider recovery tests69/69, source boundary,
configured/effective unit checks, strict MkDocs, 572-document validation,
recipe syntax and reproducible archive identity pass. The owner has not yet
confirmed physical Next Track/text delivery. No new recording, suspend,
compositor restart, pairing or selected-index helper write was exercised.

The current-boot audit records65 short s2idle cycles and6 global OOM kills;
those remain unresolved/attribution-limited. Current core service snapshots
respond. The earlier virtual-camera module-load error is not a current missing
module: the packaged module loads successfully.

## Exact remaining package gate

Desktop r31 full source build remains active on qinda as
`qindaqt-reboot-repair-build-20261017.service` (MainPID2070427,
8CPU/12GiB/no swap). Source archive SHA256:
`7c0260aeb2f14c5349c7e389b1326b5abc3313bf386cb993a5c3dabced2c1d16`.
It builds r31 and a Gabbee r8 binary; it installs neither on qinda. Do not claim
build completion from an intermediate ExecMainStatus=0 while activating.

Next action: inspect final systemd Result/ExecMainStatus and package signatures,
then adopt r31 through Portage on the laptop. Remove only the matching incident
user override after the default unit is installed, reload the user manager,
and verify default-unit activation while preserving connected devices. Source
and all recipes are preserved on qinda; no unfinished source exists only on the
laptop. The root runtime/audit work is available, not a background Codex worker.

Concrete help offer: exact artifact/adoption verification when the build ends,
and wake-IRQ/source capture for a separately owner-approved timed suspend.
