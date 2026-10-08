# ADR-0363: Admit per-volume Trash without following payload links

- **Status:** Accepted
- **Date:** 2026-10-08
- **Owners:** File Manager
- **Supersedes:** Only the home-only Trash admission and final-link refusal portions of [ADR-0064](0064-confine-file-mutation-to-identity-checked-local-authority.md)
- **Superseded by:** None

## Context

Home storage refuses cross-device and final-symlink entries. Restore Last lacks
a storage location receipt; Put Back and the Trash place recognize home only.
These are deliberate earlier boundaries, not per-volume delivery.

The [freedesktop Trash1.0 specification](https://specifications.freedesktop.org/trash/latest/)
defines sticky shared .Trash with a numeric user subdirectory and private
.Trash-uid fallback. Volume original paths are relative to the filesystem top.
Metadata is reserved exclusively before payload movement; names are unique
across both namespaces. First Path and DeletionDate values take precedence.

## Decision

Private File Manager collaborators own pure metadata, live directory admission,
and transactions. Public RemovableMedia snapshots are presentation observations,
never authority. No mount, helper, UDisks, cross-device copying into Trash,
permanent-delete fallback or automatic replay is added.

Home storage remains preferred on its filesystem. Otherwise Linux descriptor
mount observations find the filesystem top directory. The existing directory
admission pins the observed mount and reopens the no-follow pathname before
each action. A persisted ID or same-device number is not a mount incarnation.

Shared .Trash must be a real sticky directory; failed admission/creation falls
back to .Trash-uid. Private root/info/files must be real UID-owned0700
directories on the admitted filesystem. Existing foreign/permissive storage is
refused, never chmodded/repaired. Read-only discovery considers both existing
locations without creating anything. Unmounted/revoked public attachments
cannot mount automatically or revive stale-path authority.

Parent traversal uses no-follow descriptors. A final symlink is a payload inode:
rename it without resolving its target, including broken/looping links. Whole
directories move without traversing their contents. Current listing identity,
parents, storage ancestry and mounts fence every operation. Publication uses
kernel no-replace. Post-rename readback and parent sync precede success.
Unexpected capture/uncertain sync yields explicit placement uncertainty and
retains entries/evidence rather than deleting foreign replacements.

Bounded trashinfo encoding percent-escapes native bytes. Home paths may be
absolute; volume paths are relative to the admitted top. Relative components
cannot be empty, dot or dot-dot. Wrong header, malformed percent escapes, NUL,
oversized records, invalid first required values or escapes refuse.
QString paths that cannot roundtrip native encoding refuse without replacement;
no public path ABI expansion occurs. Record decoding grants no restore authority.

Restore Last retains payload/storage observations. Put Back re-admits storage
and rereads metadata. A graphical dialog offers explicit choice of an existing
local folder when the original parent is gone; chosen restore keeps the metadata
basename and freshly observed parent. No silent parent creation, collision
rename, overwrite of dangling links or directory merge. Cross-device chosen
restore refuses and preserves the payload.

Failure cleanup has no path deletion authority. Retained metadata-only orphans
are separate from available payloads and must not be mislabeled as unrestored
content. Successful restore cannot remove a foreign info replacement.
Empty Trash remains separately confirmed and home-only; volume erasure/cache
maintenance is outside this outcome. Per-item receipts keep attempted prefix,
first failure and unattempted suffix; partial effects refresh literal plain and
accessible text without claiming success.

## Acceptance

No native result is established by source/fake checks. A separate serialized
root grant freezes Debug/Release commands, input pins, private environments,
bounds and output preservation before real fixtures.

- Codec: home/volume roundtrips; newline, percent, markup, Unicode/BOM/U+FFFD;
  invalid percent/NUL/traversal/absolute volume/first duplicates/date/bounds;
  bad native byte/unpaired-surrogate refusal and atomic destination.
- Actual distinct-device private trees: home preference, sticky shared store,
  private fallback; real topology read-only mount-root observation. Injected
  private test roots never disguise same-device controls as volume success.
- Admission: symlinked root/info/files, nonsticky shared, foreign ownership,
  unsafe permissions, read-only/full/unwritable storage, disconnect/replacement.
  Preserve source/foreign entries; never mutate mounted user volumes/host data.
- Transactions: files/directories/repeated names/orphan collisions; external,
  broken and looping symlink payloads with unchanged targets; no-replace races;
  source/parent/storage/metadata replacement and uncertain rename/sync receipts.
- Restore: original path, vanished parent and deliberate chosen folder,
  dangling-link collision, cross-device refusal and parent replacement.
  Verify source/payload placement and every foreign replacement after failure.
- Controller/UI: partial batch prefixes, cancellation and untouched suffix;
  restart fresh discovery and deliberate restore; literal hostile path,
  keyboard/accessibility and explicit chosen-folder controls.
- Public-media consumer: mounted read-only discovery, unavailable rows,
  attachment/owner loss, and unchanged unrelated device actions.
- Strict focused Debug/Release CTests, normal owning registry configure,
  strict wiki/link checks and independent exact storage review before integration.
  Installed GUI and physical device evidence remain separate.

## Consequences

Accepted [ADR-0357](0357-preserve-failed-copy-output-without-cleanup-authority.md)
copy retention and [ADR-0355](0355-preserve-source-bytes-during-cross-device-moves.md)
Move recovery remain unchanged. Exact candidate0273 received independent source/native acceptance7b02284cf. Manager integration passed strict Release production build and22/22 Trash plus move-recovery rows (298Qt checks and12pure controls, no failures/skips). Author Debug evidence remains separately recorded. Installed delivery, physical-device journeys and the remaining symlink Copy packet are not established by this acceptance.

Native local listing checks raw names before granting mutation identities. Bad-byte names and any literal-U+FFFD alias remain visible without mutation identity; a bounded incomplete scan admits no identities. Display and admission use the same native enumeration. Valid Unicode, newline and percent names remain representable.
