# Display color model

The Display Color C0 `display_color_model` candidate owns the pure value model
for per-output color management: injected ICC profile descriptors with bounded
header validation, validated import metadata, deterministic catalog ordering,
per-output capability and assignment-intent values, truthful degraded states,
and a fingerprinted atomic snapshot with fail-closed lineage. It applies no
profile, mutates no compositor or display, reads no host color configuration,
and claims no HDR/ICC *application*; every consumer is a later, separately
reviewed lane. Focused build/tests pass; until independent exact-commit review
accepts this preserved boundary, this page remains a normative candidate
contract rather than integrated evidence. The recovery that finished this
slice preserved the original Solene Ward model bytes and repaired only the
defects recorded in the handoff.

## Module boundary

The module is Qt Core value types plus one non-Qt-object model class. It owns
no event-loop object, thread, timer, file, IPC connection, compositor access,
or display hardware handle, and it never links the sibling Display1 protocol,
identity, topology, transaction, or service modules: a stable ID is accepted
by bounded opaque format, not by importing Display1 identity code. A
source-policy test row rejects forbidden dependencies in every production
file, and a poison-negative row plants a forbidden include in a disposable
copy to prove that policy is not vacuous. The decision record is
[ADR-0030](../adr/0030-display-color-c0-model-boundary.md).

## Values and bounds

All text limits are UTF-8-safe character counts; counts are checked before
any derived allocation. Hostile aggregate inputs — NaN/infinite luminance,
arbitrary enum casts, oversized lists — fail closed.

| Value | Bound |
| --- | --- |
| ICC profile byte size | 128 through 4,194,304 bytes (4 MiB) |
| ICC header buffer | at least 128 bytes, never larger than the declared total size |
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
supplied buffer not larger than that total size; the version major byte in
the known published generations 2 through 5; the profile/device class among
the seven standard ICC classes; the data color space `RGB ` or `GRAY`; the
connection space `XYZ ` or `Lab `; and the `'acsp'` magic at byte 36. A valid
summary extracts declared size, CMM type, version, class, spaces, and the
16-byte MD5 profile ID without interpreting profile body tags. Descriptor
validation additionally checks identifiers, names, file name safety, enum
ranges, size consistency, and checksum length. The checksum is provenance
metadata: C0 has no profile body bytes, so digest *computation* is a later
import lane's obligation.

## Deterministic catalog

`normalizeAndSortCatalog` filters descriptors that fail validation,
deduplicates by profile ID keeping the first occurrence, caps at 256, and
sorts by origin (BuiltIn first), then case-insensitive display name, then
exact profile ID. Two models fed the same profiles in different orders
therefore publish byte-identical catalogs, and the default sRGB profile is
the caller's validated choice or deterministically the first sorted entry.

## Assignment intent and degraded truth

`ColorModel` keeps capabilities, requested assignments, applied assignments,
and a last-known-good assignment per output stable ID. Requested intent is
immutable truth for observers. Applied truth is re-derived deterministically
from the complete current model, so identical inputs yield identical
snapshots:

- HDR policy on an output without HDR capability degrades truthfully to SDR
  sRGB; WCG likewise. A capability-clamped applied assignment also falls back
  to the default sRGB profile, never the requested HDR/WCG profile, so
  published state stays coherent and fail-closed.
- A requested profile that is missing degrades with `ProfileNotFound` and
  falls back to the last-known-good assignment while it still resolves,
  otherwise the default sRGB profile.
- A profile that has become invalid degrades with `ProfileInvalid` and falls
  back to the default sRGB profile.
- A valid, non-degraded resolution becomes the new last-known-good.

## Lineage and atomic publication

Every snapshot carries schema version 1, a non-empty service epoch, a
model-monotonic revision, and a SHA-256 lineage fingerprint over the sorted
catalog and sorted output states. `validateLineage(epoch, revision)` accepts
exactly the current pair: stale (older) and out-of-order (newer) foreign
revisions both fail closed, and revisions are never ordered across epochs.
`resetEpoch` starts a fresh epoch at revision zero. Mutators validate their
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
version bounds, spaces/classes, buffer-size consistency, summary truth),
catalog (descriptor metadata, path-traversal safety, hostile enum casts,
deterministic sorting, dedup, capacity), model (epoch/lineage exact
equality, deterministic fingerprint, degraded HDR/WCG/missing/invalid
profile fallbacks, output lifecycle, epoch reset, hostile NaN/enum/list
atomic rejection, 32-output aggregate cap), the source-policy boundary row,
its poison-negative proof, and an installed staged-header C++ consumer.
Every row is deterministic model evidence; none is transport, compositor,
display, or color-application evidence.
