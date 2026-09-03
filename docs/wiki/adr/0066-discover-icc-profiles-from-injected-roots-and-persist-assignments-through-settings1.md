# ADR-0066: Discover ICC profiles from injected roots and persist color assignments through Settings1

- **Status:** Accepted
- **Date:** 2026-09-03
- **Owners:** Display Color lane (Platform services workgroup)
- **Supersedes:** None
- **Superseded by:** None

## Context

[ADR-0046](0046-display-color-c0-model-boundary.md) left the Display Color C0
model without any file, transport, or persistence authority: live profile
discovery, user import, and per-output assignment persistence were explicitly
later slices. This slice delivers them and must decide two durable questions:
where import/discovery gets its filesystem authority, and where assignment
intents live. The owning architecture page is
[Display color model](../architecture/display-color-model.md).

Discovery that resolved standard directories itself (HOME, XDG, hardcoded
`/usr/share`) would hide authority, make private-session tests host-dependent,
and read the developer's real profile directories — the exact problem
[ADR-0051](0051-persist-display-journal-in-injected-state-root.md) refused for
the Display1 journal. A second decision point: per-output assignment could be
persisted through Settings1 or through a new private journal file. A private
journal would duplicate Settings1's revision authority, conflict detection,
and no-replay semantics, and would contradict the accepted Display boundary
that Settings owns policy persistence while KWin owns live state.

## Decision

Discovery and import read the host filesystem only through roots injected by
the composition root. `display_color_discovery` never resolves HOME, XDG
variables, or default color directories; production composition supplies the
system directories and the per-user ICC directory explicitly. Scan bounds
(files per root, tag-table entries, description tag bytes) are contract:
exceeding them degrades the catalog with diagnostics instead of unbounded I/O.
Discovery reads only the 128-byte ICC header plus a bounded description tag
('desc' or 'mluc'); it never interprets the profile body. A discovered
profile's identity is its sanitized file stem, its origin is exactly the
origin of the injected root it was found under, and its color semantics are
unproven placeholders that can never satisfy the C0 truthful-sRGB-default
rule until a consumer classifies them. Each root is canonicalized once before
enumeration. A `..` component, canonicalization failure, mismatch between its
lexical absolute and canonical paths (including any symlink component), or
unknown injected origin rejects it. Every discovery candidate must canonicalize
to a lexical descendant of the canonical root before it is inspected.
Ordinary discovery reads no profile body beyond bounded metadata; candidates
whose IDs and inspected metadata collide are compared byte-for-byte in bounded
chunks solely to distinguish exact duplicates from conflicting content.

User import validates the complete source before any mutation, computes the
SHA-256 content digest as the profile lineage fingerprint, and stores the copy
in the injected user root through the ADR-0051 durability pattern: an existing,
no-symlink-component, effective-user-owned, non-group/other-writable root; an exclusive
mode-0600 temporary name; fsync; an atomic rename commit point; and a
directory barrier where supported. Import retains that one canonical root for
the operation and requires the canonical destination — or the canonical parent
of a destination that does not exist yet — to remain lexically inside it. Root
and containment rejection precede every destination stat, read, digest, or
mutation. A dot-prefixed destination name is refused,
because discovery ignores dot names and such a file would otherwise be stored
yet never re-enter the catalog. The same round-trip rule refuses any suffix
other than case-insensitive `.icc` or `.icm`. Rejection is atomic, and an interrupted
write leaves only a dot-prefixed temporary that the next import removes and
discovery ignores.

Per-output assignment intents persist as the `displays.colorAssignments`
Settings1 value through the documented, strictly validated document shape
(mapped by the [Settings1 reference](../reference/settings1-v1.md)), driven by
`display_color_assignment` through the public Settings1 client. Draft
application is optimistic and fenced: conflict and epoch truth come from the
service, an uncertain write is reported and never replayed, and a persisted
document that does not decode fails closed instead of being overwritten. The
C0 revisioned snapshot remains the only applied-assignment authority —
persistence records intent only. A record whose output is currently disconnected
is retained until an explicit removal draft; transient connectivity is not
persistence authority.

## Consequences

- Tests need no host environment: roots, files, and the transport seam are
  injected fakes, and no test reads real profile directories.
- The discovery catalog is deterministic across processes and restarts; two
  scans of the same roots publish the same bytes, and conflicting duplicate
  identifiers drop both entries order-independently, matching C0.
- The import lane owns digest computation, which C0 deliberately deferred;
  the fingerprint enables a later application lane to verify that assigned
  bytes equal assigned provenance.
- Until a later lane classifies real gamut/transfer semantics, every
  discovered profile is a non-sRGB placeholder in the C0 model, and the
  truthful sRGB default stays empty rather than guessing.
- Settings1 remains the single persistence authority for color assignment;
  there is no second journal format to migrate, and the service's revision
  conflict and epoch semantics are reused instead of reinvented.
- A hostile or unknown assignment document degrades to
  `UnusableDocument`; draft applies refuse rather than overwrite unknown
  formats. Resolution belongs to an explicit future migration.

## Revisit when

A color-management application lane needs profile-body interpretation (full
tag parsing, verification of checksums against content) beyond the import
digest; or per-output records must carry runtime policy (for example SDR
brightness) that cannot fit the strict record shape; or Settings1 loses its
role as the session settings authority. Convenience path discovery alone is
not sufficient reason to revisit.
