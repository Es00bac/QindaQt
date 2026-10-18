# Post-reboot service audit — October 17, 2026

The owner reported unavailable Bluetooth and a Next Track dictation key that
did nothing. This audit concerns the physical laptop session booted on October
16 at 21:42 MDT, with Desktop r30, compositor r7 and Gabbee r7 already installed.
It does not establish that the stability release fixed every daily-use journey.

## Observed failures and recovery

| Area | Direct evidence | Current boundary |
| --- | --- | --- |
| Bluetooth radio | The HCI radio was soft-blocked; BlueZ reported `off-blocked`. The installed helper exited with status 1. Its effective sandbox returned `EACCES` connecting to the ordinary session bus. | Unblocking only the selected HCI index restored `Powered=true`; a controller subsequently connected. The reviewed sandbox setting is applied to the existing installed unit through a scoped per-user override. That effective unit owns BluetoothRadio1 and runs successfully; a public SetPowered(true) request succeeds while the controller remains connected. Desktop r31 completed and its authenticated image underlies the installed r32 controller repair; the matching scoped helper override remains effective pending separate default-unit verification. |
| Dictation shortcut | Voice1 was idle and enabled, but advertised `Media Previous`. Neither Gabbee action existed in the compositor's live Shortcuts1 bindings, despite an existing portal session. | Saved the owner's `Media Next` preference and gracefully restarted only the idle provider. Both dictation and command bindings are present and active. Gabbee r8 is installed through Portage; all 262 package objects match and its installed watcher matches reviewed source. The new probe verifies both current Session endpoints, and the watchdog exits successfully. Physical-key/text-delivery confirmation remains separate. |
| Suspend | The kernel recorded 65 s2idle entries between 22:49 and 23:23, generally waking about six seconds later. `suspend-then-hibernate` returned each time. | Still unresolved. The installed 90-minute hibernation delay does not demonstrate that repeated early wakes reach hibernation. No suspend was requested during this audit. |
| Memory pressure | Six global OOM kills between 10:48 and 11:17: three Python processes and three browser content processes. | The process names do not identify the Python workloads. At inspection, about 8 GiB RAM was available and the user bus occupied about 3 MiB RSS. Do not attribute these kills to a Qinda service without evidence. |
| Virtual camera | Early module loading logged missing `v4l2loopback`. The current kernel's packaged module is present and `modprobe` succeeds. | Module is now loaded. An early boot error alone does not establish a current camera failure; no OBS camera-output journey was exercised. |

## Services responding at inspection

Audio1, Bluetooth1, Power1, Display1, Network1 and Clipboard1 returned live
snapshots. Bluetooth1 and Power1 reported ready; Display1 listed one output.
The compositor, shell, notifications, Settings1, Network secret agent, keyring,
NightLight, removable-media host, portal backends, PipeWire and WirePlumber had
live processes and bus owners. Notifications returns QindaQt server information, and Settings1 successfully
returns confirmed enabled voice-input preferences. Both system and user failed-unit
lists are empty after the live helper correction.

Presence and a responsive snapshot do not prove playback, physical pairing,
password unlock, screen sharing, removable-media writes or dictation delivery.
The watchdog is a timer-driven oneshot: inactive between successful runs is
normal. Its one earlier failure is not a current stopped-service diagnosis.
The Bluetooth helper is likewise on demand, but its failed activation was real.

## Follow-up work

1. The [helper-unit package](../architecture/bluetooth-service.md#selected-adapter-power-recovery)
   build `qindaqt-reboot-repair-build-20261017.service` finished successfully on qinda.
   Source `37988eb8f`, archive SHA256 `7c0260aeb2f14c5349c7e389b1326b5abc3313bf386cb993a5c3dabced2c1d16`,
   overlay `8dfc954`, Desktop r31. Its signed image is authenticated and inherited
   by the laptop's installed [r32 controller repair](../architecture/controller-input.md#october-17-frame-timing-acceptance).
   Default helper-unit adoption still needs verification. Remove only the
   matching incident override
   `~/.config/systemd/user/qindaqt-bluetooth-radio-helper.service.d/90-reboot-bus-visibility.conf`
   and verify the default unit again; do not remove unrelated user settings.
2. Confirm Next Track with a real dictation and retain the user's preference.
3. Retain the delivered Gabbee r8 regression checks: the provider's matching
   Session must exist at both the frontend and the pinned current native backend.
   An orphan frontend object must not rearm recovery. This does not attribute
   the observed native session loss to suspend.
4. Diagnose the s2idle wake source before claiming lid-closed battery safety.
   The `tools/diagnose-s2idle-wake` diagnostic suspends the machine;
   it needs explicit owner consent and must not run during interactive work.
5. Identify OOM-causing workloads before assigning a memory regression to a
   service or changing its limits.

See `docs/HANDOFF.md` for the released source and
[testing harness](testing-harness.md) for the distinction between isolated
checks and physical-session qualification.

## Verification receipts

Independent review accepts Desktop source `37988eb8f`, Gabbee `be99a1b`, and
overlay `8dfc954`. Owning radio CTests pass 7/7 on the integrated qinda tree;
Gabbee recovery checks pass 69/69. Unit verification, strict MkDocs, local links
and matching source archives pass. Installed Gabbee object verification is
262/262 with zero mismatches. No radio-off test was run after the controller
connected, and no new microphone recording, suspend or session restart was
requested. The current helper proof qualifies startup and an already-powered
enable request, not a newly exercised selected-index helper write.
