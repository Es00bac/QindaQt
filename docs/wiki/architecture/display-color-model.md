# Display color model

The Display Color C0 `display_color_model` candidate owns the pure value model
for per-output color management: injected ICC profile descriptors with bounded
header validation, validated import metadata, deterministic catalog ordering,
per-output capability and assignment-intent values, truthful degraded states,
and a fingerprinted atomic snapshot with fail-closed lineage. It applies no
profile, mutates no compositor or display, reads no host color configuration,
and claims no HDR/ICC *application*; every consumer is a later, separately
reviewed lane. The C1 lanes (`display_color_discovery` and
`display_color_assignment`, recorded in
[ADR-0066](../adr/0066-discover-icc-profiles-from-injected-roots-and-persist-assignments-through-settings1.md))
deliver live ICC profile discovery/import over injected roots and persistent
per-output assignment intents through Settings1; compositor application,
Settings UI, colord integration, HDR/WCG runtime claims, and physical
hardware qualification remain later slices. Focused build/tests pass; until
independent exact-commit review accepts this preserved boundary, this page
remains a normative candidate contract rather than integrated evidence. The
recovery that finished the C0 slice preserved the original Solene Ward model
bytes and repaired only the defects recorded in the handoff.

## Module boundary

The module is Qt Core value types plus one non-Qt-object model class. It owns
no event-loop object, thread, timer, file, IPC connection, compositor access,
or display hardware handle, and it never links the sibling Display1 protocol,
identity, topology, transaction, or service modules: a stable ID is accepted
by bounded opaque format, not by importing Display1 identity code. A
source-policy test row rejects forbidden dependencies in every production
file, and a poison-negative row plants a forbidden include in a disposable
copy to prove that policy is not vacuous. The decision record is
[ADR-0046](../adr/0046-display-color-c0-model-boundary.md).

## Values and bounds

All text limits are UTF-8-safe character counts; counts are checked before
any derived allocation. Hostile aggregate inputs — NaN/infinite luminance,
arbitrary enum casts, oversized lists — fail closed.

| Value | Bound |
| --- | --- |
| ICC profile byte size | 128 through 4,194,304 bytes (4 MiB), exactly equal to the header's declared size |
| ICC header buffer | at least 128 bytes, never larger than the profile's declared size |
| Profiles per catalog | 256, deterministic order, unique IDs |
| Outputs per model | 32 aggregate across capabilities and assignments |
| Profile ID / stable ID | 1 through 128 chars of `[A-Za-z0-9._:-]` |
| Display name | 1 through 128 non-blank characters |
| Description | at most 512 characters |
| File name | optional, at most 255 characters, no separators, no `..`, no control/space characters |
| SHA-256 checksum field | empty or exactly 32 bytes |
| Luminance values | finite 0.0 through 10,000.0 nits; min ≤ max; full-frame within [min, max] |
| Gamut/transfer lists | at most 5 entries each, every entry a known enumerator |

Origins are `BuiltIn`, `System`, `UserImported`, and `EdidDerived`; intents
are the four ICC rendering intents; gamuts are sRGB, DCI-P3, BT.2020,
AdobeRGB, and Custom; transfer functions are sRGB, linear, PQ, HLG, and
gamma 2.2; policies are SDR sRGB, SDR WCG, HDR enabled, and auto color
management. Every enum crossing the public API is range checked, so a value
decoded from hostile storage can never become catalog, capability, or
assignment truth.

## ICC header validation

`validateIccHeader` accepts an injected byte buffer, never a file. It checks,
in order: non-empty data; at least the 128-byte ICC header; declared profile
size within [128, 4 MiB] and not above a supplied total file size; the
supplied buffer never larger than the profile's declared size (which also
bounds it by any consistent total file size); the version major byte in
the known published generations 2 through 5; the profile/device class among
the seven standard ICC classes; the data color space `RGB ` or `GRAY`; the
connection space `XYZ ` or `Lab `; and the `'acsp'` magic at byte 36. A valid
summary extracts declared size, CMM type, version, class, spaces, and the
16-byte MD5 profile ID without interpreting profile body tags. Descriptor
validation additionally checks identifiers, names, file name safety, enum
ranges, exact size consistency — descriptor byte size, embedded declared
size, and supplied buffer must all agree — and checksum length. The checksum
is provenance metadata: C0 has no profile body bytes, so digest *computation*
is a later import lane's obligation.

## Deterministic catalog

`normalizeAndSortCatalog` filters descriptors that fail validation,
collapses exact-equal duplicates to one entry, and rejects a duplicate ID
whose descriptors conflict — both entries, in either input order, so
publication never depends on input order. It sorts by origin (BuiltIn
first), then case-insensitive display name, then exact profile ID, and caps
at 256 after sorting. Two models fed the same profiles in different orders
therefore publish byte-identical catalogs, and the default sRGB profile is
the caller's choice only when it carries truthful sRGB gamut and sRGB
transfer semantics, otherwise deterministically the first sorted entry with
those semantics; when no such entry exists the default is empty and SDR
fallbacks fail closed with no applied profile rather than publishing a
non-sRGB default.

## Assignment intent and degraded truth

`ColorModel` keeps capabilities, requested assignments, applied assignments,
and a last-known-good assignment per output stable ID. Requested intent is
immutable truth for observers. Applied truth is re-derived deterministically
from the complete current model, so identical inputs yield identical
snapshots:

- HDR policy on an output without HDR capability degrades truthfully to SDR
  sRGB; WCG likewise. A capability-clamped applied assignment also falls back
  to the default sRGB profile, never the requested HDR/WCG profile, so
  published state stays coherent and fail-closed. When no truthful sRGB
  default exists the applied profile is empty, never a non-sRGB profile.
- A requested profile that is missing degrades with `ProfileNotFound` and
  falls back to the last-known-good assignment while it still resolves,
  otherwise the default sRGB profile.
- A profile that has become invalid degrades with `ProfileInvalid` and falls
  back to the default sRGB profile.
- A valid, non-degraded resolution becomes the new last-known-good.

## Lineage and atomic publication

Every snapshot carries schema version 1, a non-empty service epoch, a
model-monotonic revision, and a SHA-256 lineage fingerprint over one
schema-tagged, domain-tagged, length-delimited canonical encoding of every
semantically published snapshot field — catalog metadata and flags,
per-profile identity/name/description/file/gamut/transfer/header/checksum/
size/flags, and per-output capabilities, requested and applied assignments,
active profile, degraded reason, and state flags. Each field is framed as
`[tag length][tag][payload length][payload]`, so no two distinct published
states can share a fingerprint, and per-field mutation regressions pin the
coverage. `validateLineage(epoch, revision)` accepts
exactly the current pair: stale (older) and out-of-order (newer) foreign
revisions both fail closed, and revisions are never ordered across epochs.
`resetEpoch` starts a distinct (or generated) epoch at revision zero;
resetting to the epoch already in force is a no-op that never regresses the
model-monotonic revision. Mutators validate their
complete input before touching state; a rejected mutation leaves revision,
snapshot, and fingerprint byte-identical (atomic reject), which regression
rows pin by comparing complete snapshots before and after hostile input.

## Focused proof

The candidate selectors are:

```sh
ctest --test-dir build/<debug|release> \
  -R '^qindaqt\.display-color-' \
  --output-on-failure --no-tests=error
```

Six rows: header validation (magic, truncation, declared-size bounds,
version bounds, spaces/classes, buffer-size consistency against the
declared size, summary truth), catalog (descriptor metadata, path-traversal
safety, hostile enum casts, exact declared/descriptor/buffer size
consistency, the ASCII identifier grammar, deterministic sorting,
order-independent duplicate handling, capacity), model (epoch/lineage exact
equality, deterministic fingerprint, full-field fingerprint mutation
coverage and framing unambiguity, degraded HDR/WCG/missing/invalid profile
fallbacks, truthful sRGB default semantics with fail-closed absence,
output lifecycle, epoch reset with same-epoch monotonicity, hostile
NaN/enum/list atomic rejection, 32-output aggregate cap), the source-policy
boundary row, its poison-negative proof, and an installed staged-header C++
consumer. Every row is deterministic model evidence; none is transport,
compositor, display, or color-application evidence.

## C1 discovery and import boundary

`display_color_discovery` turns injected filesystem roots into C0 import
metadata; the decision record is
[ADR-0066](../adr/0066-discover-icc-profiles-from-injected-roots-and-persist-assignments-through-settings1.md).
Its authority contract:

- Roots are injected by the composition root together with their origin
  (`BuiltIn` for repository-shipped profiles, `System` for the standard
  system directories such as `/usr/share/color/icc`, `UserImported` for the
  per-user ICC directory). The module never resolves HOME, XDG variables, or
  any default directory, and never reads a root it was not given.
- Scanning is flat (no recursion), accepts `*.icc`/`*.icm` case-insensitively,
  ignores dot-prefixed names, never follows file symlinks, and caps candidates
  per root. Each injected root is resolved once to its canonical path before
  enumeration; a `..` component, failed canonicalization, or mismatch between
  the lexical absolute root and canonical root rejects it (including a symlink
  in any root component). Every candidate is separately canonicalized and must
  be a lexical descendant of that canonical root before any stat or read.
  Exceeding a bound sets `complete = false` with a diagnostic instead of
  scanning forever. An unknown injected origin rejects that root before any
  enumeration; it never inherits built-in provenance.
- Per file it stats, then reads only the 128-byte ICC header plus a bounded
  tag table and description tag. A tag table or description tag that exceeds
  its scan bound is truncated or skipped with an Info diagnostic while the
  profile stays catalogable under its file-derived name. Hostile or damaged
  files — empty, truncated, garbage, mislabeled (declared size disagreeing
  with actual), declared oversize, symlinked, or C0-invalid metadata —
  produce bounded diagnostics and are skipped; they never abort the scan.
- The declared profile size must equal the actual file size exactly;
  discovered descriptors keep `checksumSha256` empty because the module never
  interprets or digests the profile body. Ordinary discovery does not read the
  remainder. Only when two candidates collide on one ID and all inspected
  metadata is equal does discovery compare their raw content in bounded chunks;
  unequal or unverifiable content drops the ID, while exact bytes collapse.
  Profile identity is the sanitized file stem through the C0 identifier
  grammar, and the display name comes from the parsed `desc`/`mluc`
  description with the sanitized stem as the deterministic fallback.
- Discovered semantics are unproven placeholders (`Custom` gamut, non-sRGB
  transfer). They can never satisfy the C0 truthful-sRGB-default rule, so a
  scanned profile cannot become the "default sRGB" until a consumer classifies
  its real gamut/transfer and publishes it through `ColorModel::setCatalog`.
- The catalog result is deduplicated and sorted through C0
  `normalizeAndSortCatalog`; conflicting duplicate identifiers (same stem,
  different content) drop both entries order-independently with a
  diagnostic, exactly as C0 would.

`importUserProfile` validates the complete source (regular, non-symlink,
readable, within [128 bytes, 4 MiB], valid header, declared equal to actual,
C0-safe destination name with a case-insensitive `.icc` or `.icm` suffix —
dot-prefixed and other suffixes are refused, because discovery ignores them and
a stored file would otherwise never re-enter the catalog — before touching the
user root, computes the
SHA-256 content digest as the lineage fingerprint (stored in the descriptor's
`checksumSha256`), and copies the exact bytes into the injected user root
through the ADR-0051 pattern: an existing root whose one canonical resolution
contains no `..` or symlink redirection and that is effective-user-owned and
non-group/other-writable; an exclusive mode-0600 temporary; fsync; one atomic
rename commit point; a directory barrier where supported. The destination, or
the canonical parent when it does not yet exist, must be lexically inside that
same canonical root before any destination stat, read, or digest. Re-importing
byte-identical content is an idempotent `AlreadyPresent`; a different file
under the destination name is a conflict that leaves the root untouched;
every rejection is atomic; an interrupted write leaves only a dot-prefixed
temporary that the next import removes and discovery ignores; a
post-commit directory-barrier failure reports `DurabilityUncertain` instead
of claiming provenance.

## C1 persistent assignment boundary

`display_color_assignment` persists per-output assignment intents as the
documented `displays.colorAssignments` Settings1 value (see the
[Settings1 reference](../reference/settings1-v1.md) and
[ADR-0066](../adr/0066-discover-icc-profiles-from-injected-roots-and-persist-assignments-through-settings1.md)).
Settings1 is the persistence authority because the schema already supports
bounded object values in the `displays` domain and supplies revision,
conflict, epoch, and no-replay semantics a second journal would only
duplicate; the C0 revisioned snapshot remains the only applied-assignment
authority. The strict document shape is one record per output stable ID with
exactly `profile` (C0 identifier grammar) and `lineage` (empty or exactly 64
lowercase hex characters — the raw SHA-256 import fingerprint's canonical
form); decoding is all-or-nothing, capped at the C0 32-output aggregate.
Records for outputs absent from the current live inventory are retained until
an explicit draft removes them. Persistence owns user intent rather than live
connectivity; transient unplug/hotplug must not silently erase that intent, and
the later application lane remains responsible for joining records to live
outputs.

`SettingsAssignmentStore` composes the public Settings1 client and reports
typed truth:

- `document()` distinguishes `Ready` (confirmed snapshot decoded into a
  usable document, with epoch and revision), `Unavailable` (no confirmed
  authority — transport lost, owner replacement, or before the first
  snapshot; the last confirmed document is retained but never reported as
  live), and `UnusableDocument` (confirmed but hostile or unknown-shape
  value, with the decode reason).
- `applyDraft` validates the draft (valid stable IDs and profile grammar,
  32-byte-or-empty lineage, no duplicate targets, output cap), refuses
  without confirmed usable authority, refuses to merge into an unusable
  document so unknown formats can never be silently overwritten, and sends
  the merged document as one optimistic transaction fenced with the
  confirmed revision.
- `applyFinished` reports `Applied` (verified against the reply's
  authoritative current value), `AppliedNoOp` (unchanged revision, no
  changed keys), `Conflict` (base revision rejected — rebase required),
  `Uncertain` (timeout, bus loss, or an invalid reply — never replayed;
  resync and re-apply explicitly), or `Failed`. A service epoch change can
  never arrive as a commit outcome; it surfaces as `Unavailable` truth plus
  `Uncertain` for any in-flight write.

The composition root gives the store its own client scoped to the assignment
key and serializes calls; the store never starts, stops, or refreshes the
client and never talks D-Bus itself. Compositor application, the Settings
surface that would edit drafts, and lineage verification of applied profile
bytes remain later lanes.

### Focused proof

The focused proof rows for C0 and C1 are documented in the
[testing harness](../development/testing-harness.md#display-color-c0-model-and-c1-discoveryassignment-proof).
