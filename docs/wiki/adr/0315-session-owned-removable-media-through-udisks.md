# ADR-0315: Session-owned removable media through UDisks

- Status: Accepted
- Date: 2026-09-30

## Context

The user requires insertion prompts and routine media mounting without a
terminal, including read-only access and remembered choices. The File Manager's
local-file authority intentionally owns no mount operations (ADR-0064). Disk
operations belong outside both the browser and shell presentation processes.

## Decision

Add `qindaqt-removable-media`, an optional session-owned Qt application that
detects eligible media through the system UDisks2 ObjectManager and presents
standard notification actions plus a compiled Controls window. UDisks performs
Mount, Unmount, encrypted Unlock/Lock, Eject, PowerOff and explicitly confirmed
Format, with the session polkit agent handling authentication. The application
never spawns mount utilities or executes media content.

Keep inventory projection, confirmed per-filesystem persistence, insertion
policy, notification transport, privileged transport and QML in distinct small
collaborators. Remembered choices are app-local atomic JSON keyed by physical
drive identity and filesystem UUID. Ask is the default. Mount/open/read-only,
ignore and remembered mount are ordinary actions; formatting always requires a
separate captured-attachment confirmation. Parent partition tables, internal,
hidden and read-only/optical media are ineligible format targets. Read-only
requests must not silently inherit writable fstab behavior.

Every mutation checks fresh inventory and the current UDisks owner. Unplug,
disc replacement and owner loss invalidate attachment tokens. No mutation is
retried after uncertainty. Safe removal serializes all sibling unmounts before
drive removal and never forces busy filesystems. Export only Activate on the
application's session-bus singleton. Private sessions explicitly disable this
physical-device child.

## Consequences

UDisks2 is a runtime dependency and filesystem formatters remain Portage-managed
providers queried through CanFormat. Ordinary media use is graphical while disk
authority and authorization remain in existing system services. Per-volume Trash,
partition editing, optical burning/playback, and File Manager volume sidebar
integration remain independently scoped work. The mount-authority deferral of
[ADR-0064](0064-confine-file-mutation-to-identity-checked-local-authority.md) is
superseded only for the separately owned removable-media application; the File
Manager's file-operation authority is unchanged.

See [Removable media](../apps/removable-media.md) for behavior, persistence,
failure handling and qualification boundaries.
