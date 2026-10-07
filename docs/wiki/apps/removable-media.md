# Removable media

`qindaqt-removable-media` handles inserted USB storage, external disks, memory
cards and optical data discs through the system UDisks2 service. It runs as a
session-owned optional child after the shell and polkit agent. Its normal
launcher entry opens the media window; `--watch` starts with that window hidden.
One session-bus instance owns detection and remembered choices. A second launch
asks that instance to show its window. No device operation uses a shell command.

## Insertion and ordinary use

Newly discovered media, including media already connected at login, prompts
once for the current attachment. The notification offers **Mount and open**,
**Mount read-only**, **Always mount this media**, and **More options** as
appropriate. Mounted media offers **Open** and **Safely remove** or **Eject**.
The media window remains available from the application launcher after a
notification is dismissed. If notifications are unavailable, it opens directly.
If the notification service changes while a prompt is pending or visible, the
media window opens for the still-connected attachment. Updating a notification
preserves its current actions; withdrawing media closes even a late notification.
It follows the current QindaQt appearance and uses keyboard-accessible controls.

The window offers mount/open, read-only mount, unmount, safe removal, an
encrypted-volume passphrase field, and formatting. Mounted directories open in
QindaQt File Manager. Optical media always mounts read-only. Blank and audio-only
discs have no data filesystem to mount; the window explains this and offers
ejection. Formatting is unavailable for optical discs and hardware read-only
media. Internal disks, ignored recovery partitions, loop devices, RAID members,
and parent partition-table devices do not become insertion/format targets.

**Safely remove** unmounts all mounted volumes on the same drive, including
hidden siblings. It locks unlocked encrypted backing volumes, then uses the
drive's advertised Eject or PowerOff operation. Drives offering neither operation
are safely unmounted. A busy volume stops the sequence with a visible error;
there is no forced unmount. Ordinary unmount affects only the selected volume.

Read-only requests use UDisks' `ro` mount option. A filesystem whose system
configuration could override these options cannot acquire a new read-only
choice through this handler. Existing writable mounts must first be unmounted.
The backend verifies a newly requested read-only mount through `QStorageInfo`
and attempts to unmount it if read-only access cannot be established. A failed
writable mount offers **Try read-only**. It never forces a dirty filesystem
writable or repairs a disk implicitly.

## Remembered choices and formatting

Each identifiable filesystem has **Ask each time**, **Always mount**, **Always
mount read-only**, and **Ignore** choices. The key combines the physical drive's
UDisks identity with the filesystem UUID, rather than its reusable device node.
Choices are stored atomically in the application's `media-choices.json` user
configuration file. The notification's Always mount choice is saved only after
its mount succeeds. The window can change a saved choice back to Ask. Media
without a stable identity remains usable but cannot save an automatic choice.
Multiple remembered mounts run sequentially. Automatic insertion never formats
or executes content from a disk.

Formatting opens a separate dialog naming the captured device and its size.
The user must explicitly unmount it, select an available filesystem and type
the exact device name before confirming erasure. UDisks' `CanFormat` determines
which of exFAT, FAT and ext4 are actually available. The backend confirms current
attachment and unmounted state again before `Block.Format`. Labels are bounded;
no tear-down, whole-disk partition replacement or recursive erase is requested.
Unplugging/replacing media invalidates the confirmation even when Linux reuses
the same device name. Failed/uncertain operations are visible and are never
automatically replayed. Authentication stays with the session's polkit agent.

## Installed startup and packaging

Portage's `gui-apps/qindaqt-removable-media` component owns the executable and
launcher. Its XDG autostart entry starts `--watch` on older installed QindaQt
sessions; the session-bus singleton prevents duplicate watchers when the new
supervisor also starts it. Shared Controls/Tokens runtime remains desktop-owned.
The component supports the shared pre20260927-r7 desktop floor, and UDisks plus
FAT/exFAT/ext4 format providers are Portage dependencies. A full-desktop recipe
must set `QINDAQT_BUILD_REMOVABLE_MEDIA=OFF` while this package owns the component.

## Boundaries and evidence

The application separates pure inventory projection, atomic choice persistence,
presentation policy, notification transport, UDisks transport/operations, and
compiled QML. UDisks remains the privileged device authority. The public
`org.qindaqt.RemovableMedia1` session endpoint exports only `Activate`, not mount
or format methods. Attachment tokens are revoked on unplug and owner change;
delayed results cannot open or act on replacement media.
Every mount follow-up rechecks the attachment before read-only verification or
rollback. Safe-removal steps retain the physical drive/media generation, even
when locking a decrypted volume intentionally removes its cleartext child.

Private desktop harnesses pass `--no-removable-media` to the supervisor.
`SessionProcessOptions` leaves the helper unset unless production main configures
it. Policy, UDisks, QML and session-lifetime tests use fixture storage/private
buses; they never format physical disks. See the [testing harness](../development/testing-harness.md).
Hardware insertion/ejection and destructive formatting require disposable-media
qualification; fake-bus tests do not establish that physical evidence.

The [File Manager](file-manager.md) browses the resulting local directories.
Its own per-volume Trash/sidebar roadmap remains separate from this handler.
[ADR-0315](../adr/0315-session-owned-removable-media-through-udisks.md) records
the process and persistence choice. The upstream contracts are the UDisks
[Filesystem](https://storaged.org/doc/udisks2-api/latest/gdbus-org.freedesktop.UDisks2.Filesystem.html),
[Block](https://storaged.org/doc/udisks2-api/latest/gdbus-org.freedesktop.UDisks2.Block.html),
and [Drive](https://storaged.org/doc/udisks2-api/latest/gdbus-org.freedesktop.UDisks2.Drive.html) interfaces.

## Everyday workflow follow-up

The [October 7 audit](../development/everyday-desktop-audit.md) distinguishes
this installed helper from File Manager device discovery and the still-unverified
physical insertion/eject journey. Proposed ED-04 in the
[delivery plan](../development/everyday-desktop-plan.md) must preserve UDisks
authority and this module ownership. [Proposed ADR-0350](../adr/0350-share-removable-media-with-file-consumers.md)
defines a bounded public inventory/client and ordinary-action design for File
Manager and the native chooser. It preserves Activate compatibility, captured
attachment fencing, owner-only preferences/passphrase/formatting and authoritative
sibling-safe removal. It adds no public API or sidebar feature before review.
