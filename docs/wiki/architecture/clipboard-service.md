# Clipboard service

The clipboard architecture separates ordinary Wayland selection transfer —
which stays entirely with the compositor and toolkits — from an optional,
bounded, privacy-aware clipboard *history* that QindaQt owns. This page is
the contract for both. Clipboard C0, the first slice, delivers only the pure
model library and its codecs; it deliberately performs no host-clipboard,
Wayland, D-Bus, or UI integration, and nothing in it should be read as a
claim of live clipboard functionality.

## Outcome and slices

- **C0 (this slice, model):** bounded entry/value types, canonical MIME
  metadata and size limits, volatile opt-in history with deterministic
  eviction/dedup/pinning/clear, sensitive/one-time/non-storable refusal,
  stale-generation rejection, deterministic bounded metadata search, explicit
  ownership/lifetime/error/lineage-exhaustion contracts, and value/descriptor
  codecs plus fixtures as the seam a future adapter composes. Static unit
  evidence only; C0 alone is not the integrated searchable-history user
  outcome — that remains gated on the C1 slices below.
- **C1 (later slices, reviewed separately):** `qindaqt-clipboard-host`
  resident process, `org.qindaqt.Clipboard1` private authenticated bus
  surface, `ext-data-control-v1` adapter, Settings1 `services.clipboardHistory`
  opt-in wiring, lock-state provisioning, and presentation. C1 owns the
  *user-facing* search semantics on top of the model's metadata search:
  it gates queries behind authenticated lock state, exposes them over the
  private bus, and decides whether payload-derived search beyond the
  preview/label fields is offered at all. C1 must not begin until the
  integrated Settings1, authenticated lock state, and verified KWin protocol
  support exist, per the platform-services lane plan. Installed-header/link
  consumer evidence and packaged qualification are likewise C1 integration
  gates, not C0 claims.

## Volatile, bounded history

History is session memory only. There is no disk persistence, no
synchronization, and no export path in any slice. Every dimension that could
grow has a fixed protocol bound, defined once in
`clipboard_types.h` and reused by the model, codecs, and the future wire
protocol:

| Bound | Value | Applies to |
| --- | --- | --- |
| `kMaxEntries` | 64 | history length |
| `kMaxPinnedEntries` | 8 | pinned items |
| `kMaxFormatsPerItem` | 8 | formats per item |
| `kMaxMediaTypeLength` | 127 | canonical media type string |
| `kMaxSourceLabelCodeUnits` | 64 | producer label |
| `kMaxPreviewCodeUnits` | 96 | text/plain preview excerpt |
| `kMaxItemPayloadBytes` | 1 MiB | payload bytes per item |
| `kMaxTotalPayloadBytes` | 8 MiB | payload bytes across history |

A history instance may narrow these bounds through `HistoryLimits` but can
never widen them: the constructor clamps every field through
`sanitizeLimits`, so the protocol ceilings hold in Release builds without
relying on assertions, and a diagnostic `HistoryCounters` constructor seam
exists only for lineage-boundary tests.

## Canonical media metadata

Producer media-type spellings are never stored or encoded as-is.
`canonicalizeMediaType` lowercases, trims, and shape-checks each name:
exactly one `type/subtype` pair, or a bare vendor marker token, restricted to
`[a-z0-9+._-]`, with parameters, embedded whitespace, wildcards, and
over-length names refused outright.

Classification is an allowlist, so unknown future types fail closed:

- **Sensitive** — `x-kde-passwordmanagerhint`,
  `application/x-qindaqt-secret`. Refused for history storage always.
- **One-time** — `x-qindaqt-one-time`. Refused for history storage; the live
  selection still works normally through Wayland.
- **Storable** — `text/plain`, `text/html`, `text/uri-list`, `image/png`,
  `image/jpeg`, `image/bmp`, `image/gif`.
- **Non-storable** — everything else, including any non-canonical spelling.

When one value mixes classes the refusal precedence is sensitive → one-time →
non-storable, accumulated across **all** formats before refusing, so identical
values always produce identical errors regardless of producer ordering.
Admission also refuses empty values (no formats, or no format carrying any
payload), duplicate canonical media names, and oversized items — and it
measures declared sizes before copying any payload byte, so a hostile value
can never force allocation before it is refused.

## Privacy, opt-in, and generation fencing

The model starts fail-closed: history disabled and privacy `Denied`. Nothing
is admitted, nothing is disclosed, and snapshots return empty content with
the flags that explain why — not even aggregate byte totals are exposed while
withheld. The future host sets privacy `Allowed` only from authenticated
unlocked state, mirroring the notification presentation gate.

- Disabling the history or denying privacy **purges every entry and raises
  the generation by exactly one**. Re-stating the current authority value is
  a no-op.
- Every content operation carries the caller's `expectedGeneration` and is
  refused with `StaleGeneration` on mismatch, so any decision made before a
  purge — including an in-flight admission — is rejected instead of touching
  post-purge state. Entry ids embed their generation, so pre-purge ids can
  never resolve afterwards even if serials repeat.
- Fixed-width lineage counters fail closed: when serial, revision, or
  generation lineage is exhausted, content operations return
  `LineageExhausted` instead of wrapping. A purge at the generation ceiling
  still destroys content unconditionally — privacy purges are never refused
  — and pins the counter, after which every further content operation
  refuses. Zero or duplicate `EntryId` values are unreachable by
  construction.
- Refusal order is fixed and tested: `HistoryDisabled`, `PrivacyDenied`,
  `StaleGeneration`, `LineageExhausted`, then value validation, then
  capacity.

Payload bytes leave the model only through `promote()` — the model-level
analogue of "full data moves only after an explicit user action". Snapshots
and descriptors carry metadata plus a bounded preview; they never carry
complete payloads.

## Deterministic model behavior

- **Order:** entries are stored most-recent-first; admission and promote
  prepend. `lastUsedTick`/`admittedTick` are caller-supplied monotonic
  metadata; the model owns no clock.
- **Eviction:** while over entry or byte limits, the least-recent *unpinned*
  entry is evicted. Pinned entries are skipped; if pins alone cannot make
  room the admission is refused with `CapacityRefused` and nothing changes.
  Victims are precomputed on shadow state and only removed once the fit is
  proven, so a refusal is fully atomic: entries, byte totals, and revision
  are exactly as before, even when pins block part of the needed space.
- **Dedup:** an item whose SHA-256 fingerprint (over canonical media names,
  payload lengths, and payload bytes in stored order) matches an existing
  entry moves it to most-recent, refreshes caller metadata, and keeps its
  identity and pin. Byte totals are unchanged.
- **Pinning:** bounded by `maxPinnedEntries`; survives eviction and
  unpinned-only clears.
- **Clear:** `UnpinnedOnly` keeps pins; `All` removes everything including
  pins. Clearing is a user action, not an authority transition, so it never
  raises the generation. A clear that removes nothing is a successful no-op
  that does not advance the revision.
- **Revision:** advances by exactly one per successful *content* change
  (admission, dedup move, promote, pin change, removal, clear that removes
  at least one entry) within one generation. Non-purging authority transitions
  change only the snapshot flags. Disabling or denying authority also purges
  content, but intentionally leaves the old revision unchanged because the
  generation bump invalidates that entire lineage. Consumers observe authority
  through `historyEnabled`/`privacyAllowed` plus the generation; refusals and
  no-ops change nothing.

## Bounded metadata search

`search(query, expectedGeneration, maxResults)` performs a deterministic,
case-insensitive substring match against exactly two bounded metadata
fields per entry: the sanitized source label and the bounded preview.
Payload bytes are unreachable from search, so a presentation layer can offer
find-as-you-type without payload authority. Matches return most-recent
first, capped at the sanitized `maxResults` with `truncated` reporting
additional hits; empty queries refuse with `EmptyValue`, oversized queries
with `OversizedValue`. Search is gated exactly like every other content
operation (disabled history, denied privacy, stale generation, exhausted
lineage all refuse) and is a pure read: it never advances the revision.
User-facing search semantics — payload-derived matching, ranking, and the
private-bus surface — belong to the C1 slices, which must gate them behind
authenticated lock state.

## Codecs and the adapter seam

Two canonical, versioned byte forms exist so the future C1 adapter never
invents its own serialization:

- **Value codec** (`QCBV` format 1): bounded inline payloads for round-trip
  and testing. Encode and decode enforce identical rules — count ceiling,
  canonical media, duplicate rejection, non-empty payload, per-item and
  aggregate size ceilings — in the same error vocabulary, and both measure
  declared sizes before copying or appending payload bytes. An accepted
  encoding always decodes; large transfers still move by FD and never
  through this form.
- **Descriptor codecs** (`QCBD` entry, `QCDL` list, format 1): metadata-only
  — identity, ticks, pins, sanitized source label, bounded preview, format
  names with byte counts, and the fingerprint. These are the intended basis
  of the C1 snapshot wire form, so presentation never writes its own
  serialization. A descriptor list reports `TooManyEntries` (not
  `TooManyFormats`) when its entry count exceeds the protocol bound.

Both forms share one validation floor, centralized in a single descriptor
validator used by encode and decode alike: valid generation-tagged identity,
nonempty bounded canonical format list with unique names, non-negative
per-format and aggregate claimed bytes with at least one nonzero payload claim,
exact fingerprint width, producer metadata that already satisfies the
sanitization contract (labels and previews carry no control or format
characters and encode as canonical UTF-8), and a truncation flag that is never
paired with an empty preview.
The decoders additionally enforce the hostile-input framing floor:
little-endian fixed framing, unknown version refusal, declared lengths trusted
only when the remaining buffer satisfies them, trailing bytes refused,
canonical media re-validated on decode, source-label and preview UTF-8 required
to round-trip byte-for-byte, and unknown flag bits refused so future extensions
cannot be silently misread by an older decoder. The one property deliberately
*not* wire-enforced is the exact clamp width behind a truncation flag — that
width is instance-relative and unknowable to a peer.

## Boundaries

`clipboard_model` depends on Qt Core only. It contains no QObject, no IPC,
no Wayland, no persistence, and no clock. The model is not thread-safe: the
owning thread (the future host's Wayland/Qt main thread) must confine all
calls or provide external synchronization; returned values are safe to cross
threads after the call returns. The C1 host will compose it with transport
and lock-state authority; C0 consumers (tests today, the C1 host later) link
`QindaQt::ClipboardModel`. Raw clipboard content must not appear in logs,
diagnostics, board messages, or repository tests beyond obviously synthetic
fixtures.

Module-boundary and dependency-direction rules are in
[Module boundaries](module-boundaries.md). The durable decisions — volatile
history, allowlist storage, purge-on-privacy-loss with generation fencing,
and the pure-model seam ahead of the Wayland adapter — are recorded in
[ADR-0031](../adr/0031-volatile-bounded-clipboard-history.md).
