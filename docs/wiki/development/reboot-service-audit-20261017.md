# Post-reboot service audit — October 17, 2026

The owner reported unavailable Bluetooth and a Next Track dictation key that
did nothing. This audit concerns the physical laptop session booted on October
16 at 21:42 MDT, with Desktop r30, compositor r7 and Gabbee r7 already installed.
It does not establish that the stability release fixed every daily-use journey.

## Observed failures and recovery

| Area | Direct evidence | Current boundary |
| --- | --- | --- |
| Bluetooth radio | The HCI radio was soft-blocked; BlueZ reported `off-blocked`. The installed helper exited with status 1. Its effective sandbox returned `EACCES` connecting to the ordinary session bus. | Unblocking only the selected HCI index restored `Powered=true`. The helper-unit correction keeps home/runtime hidden with a tmpfs and exposes only the bus socket. Installed Portage qualification remains required. |
| Dictation shortcut | Voice1 was idle and enabled, but advertised `Media Previous`. Neither Gabbee action existed in the compositor's live Shortcuts1 bindings, despite an existing portal session. | Saved the owner's `Media Next` preference and gracefully restarted only the idle provider. Both dictation and command bindings are now present and active. Physical-key/text-delivery confirmation remains separate. |
| Suspend | The kernel recorded 65 s2idle entries between 22:49 and 23:23, generally waking about six seconds later. `suspend-then-hibernate` returned each time. | Still unresolved. The installed 90-minute hibernation delay does not demonstrate that repeated early wakes reach hibernation. No suspend was requested during this audit. |
| Memory pressure | Six global OOM kills between 10:48 and 11:17: three Python processes and three browser content processes. | The process names do not identify the Python workloads. At inspection, about 8 GiB RAM was available and the user bus occupied about 3 MiB RSS. Do not attribute these kills to a Qinda service without evidence. |
| Virtual camera | Early module loading logged missing `v4l2loopback`. The current kernel's packaged module is present and `modprobe` succeeds. | Module is now loaded. An early boot error alone does not establish a current camera failure; no OBS camera-output journey was exercised. |

## Services responding at inspection

Audio1, Bluetooth1, Power1, Display1, Network1 and Clipboard1 returned live
snapshots. Bluetooth1 and Power1 reported ready; Display1 listed one output.
The compositor, shell, notifications, Settings1, Network secret agent, keyring,
NightLight, removable-media host, portal backends, PipeWire and WirePlumber had
live processes and bus owners. Notification server information and narrowly
requested Settings values are appropriate read-only follow-up checks.

Presence and a responsive snapshot do not prove playback, physical pairing,
password unlock, screen sharing, removable-media writes or dictation delivery.
The watchdog is a timer-driven oneshot: inactive between successful runs is
normal. Its one earlier failure is not a current stopped-service diagnosis.
The Bluetooth helper is likewise on demand, but its failed activation was real.

## Follow-up work

1. Deliver the [helper-unit repair](../architecture/bluetooth-service.md#selected-adapter-power-recovery)
   through Portage and verify the effective installed sandbox and bus owner.
2. Confirm Next Track with a real dictation and retain the user's preference.
3. Make the dictation watchdog distinguish a portal session from actual active
   bindings after the native shortcut backend is replaced.
4. Diagnose the s2idle wake source before claiming lid-closed battery safety.
   The `tools/diagnose-s2idle-wake` diagnostic suspends the machine;
   it needs explicit owner consent and must not run during interactive work.
5. Identify OOM-causing workloads before assigning a memory regression to a
   service or changing its limits.

See `docs/HANDOFF.md` for the released source and
[testing harness](testing-harness.md) for the distinction between isolated
checks and physical-session qualification.
