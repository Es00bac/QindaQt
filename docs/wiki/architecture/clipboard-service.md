# Clipboard service

The clipboard architecture separates ordinary Wayland selection transfer —
which stays entirely with the compositor and toolkits — from an optional,
bounded, privacy-aware clipboard *history* that QindaQt owns. Clipboard C0
provides the pure volatile model and C1 composes it in a resident process with
a bounded Wayland capture adapter and private Clipboard1 bus. No slice persists
clipboard bytes or grants UI code direct payload authority.

## Outcome and slices

- **C0 (model, current):** bounded entry/value types, canonical MIME
  metadata and size limits, volatile opt-in history with deterministic
  eviction/dedup/pinning/clear, sensitive/one-time/non-storable refusal,
  stale-generation rejection, deterministic bounded metadata search, explicit
  ownership/lifetime/error/lineage-exhaustion contracts, and value/descriptor
  codecs plus fixtures as the seam a future adapter composes. Static unit
  evidence only; C0 alone is not the integrated searchable-history user
  outcome — that remains gated on the C1 slices below.
- **C1 (service, current):** `qindaqt-clipboard-host`, the
  `org.qindaqt.Clipboard1` private-bus surface and exact-owner client,
  `ext-data-control-v1` capture, Settings1 opt-in, and authenticated lock
  gating. C1 exposes only C0 descriptor bytes in snapshots; complete payloads
  move back to Wayland only after a fenced `Copy` intent. Presentation and
  metadata search UI remain outside this slice.

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
  declared sizes before copying or appending payload bytes. Decode performs a
  complete bounds/shape scan before payload materialization and publishes the
  value only after full success, so every refusal returns empty content rather
  than a valid prefix. An accepted encoding always decodes; large transfers
  still move by FD and never through this form.
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
width is instance-relative and unknowable to a peer. Descriptor-list decode
also stages entries until the entire list succeeds, so a late framing or nested
entry failure exposes no accepted prefix.

## C1 capture and resident authority

`clipboard_wayland_adapter` is a client of the pinned staging
`ext-data-control-v1` version 1 XML. It binds one seat and one manager, observes
both regular and primary selections, and reads an offer only after all
advertised MIME names have been classified. Sensitive or one-time markers
refuse the entire offer without opening a payload pipe. Unknown formats are
ignored; the remaining canonical storable formats are deduplicated and capped
at the C0 format and one-MiB aggregate item bounds. A transfer exceeding the
bound is discarded. The adapter has no `wlr-data-control` fallback: absence or
replacement of either required global withdraws availability truth instead of
silently selecting another protocol contract.

The production adapter uses a private Wayland connection and authenticates its
peer with `SO_PEERCRED`. The resulting compositor PID is the only PID accepted
by the injected `SessionLockState` monitor. Capture is enabled only while all
three facts are simultaneously true:

1. Settings1 has confirmed `services.clipboardHistory` as a Boolean `true`;
2. authenticated lock state is conclusively `Unlocked`; and
3. the data-control device is available.

Startup, Settings1 uncertainty, owner loss, lock transition, and protocol loss
all fail closed. Turning the opt-in off or losing unlocked truth purges the C0
model before readable metadata can be published. The schema currently declares
the key with a legacy default of `true`; the host deliberately does not infer
consent from that declaration and remains disabled until Settings1 returns a
confirmed value. Changing the schema default to off is a separate schema-owner
migration and remains required before default-on installations can claim the
intended product default.

`clipboard_service` owns the model, all payload bytes, the adapter, and the
private D-Bus name in one Qt event-loop thread. It installs a D-Bus activation
file and a hardened systemd user unit. There is no state directory, recovery
journal, payload logging, or disk codec. The bus object keeps a bounded
per-unique-caller request cache: repeating the same request identity returns
the original result, while reusing an identity for different arguments is
rejected. Losing a caller's unique name drops only that caller's cache.

## Clipboard1 and client fencing

Clipboard1 snapshots carry `(epoch, generation, revision)` plus C0's canonical
`QCDL` descriptor list. Epoch changes when the resident host restarts;
generation changes on an authority purge; revision changes for content within
one generation. Every Select, Delete, Clear, and Copy intent includes all three
expected values and a nonzero request id. Select promotes an item, Delete
removes it, Clear removes unpinned or all entries, and Copy promotes then
publishes the selected bounded value through Wayland. No method returns payload
bytes.

The public client is asynchronous and binds replies and `Changed` signals to
the exact unique owner. It atomically validates descriptor framing and entry
generation before publishing a snapshot, coalesces invalidations, serializes
mutations, and treats timeout, transport loss, owner replacement, malformed
wire data, and contradictory lineage as uncertain or unavailable. It never
replays a mutation. See the normative wire details in
[Clipboard1 version 1](../reference/clipboard1-v1.md).

## Boundaries

`clipboard_model` remains Qt-Core-only. `clipboard_protocol` owns only bounded
values, validation and D-Bus marshalling. `clipboard_client` depends on that
public protocol and an injected transport, never on the host. The Wayland
adapter depends on the model's canonical MIME/value vocabulary but not history
policy or D-Bus. `clipboard_service` alone composes these pieces with Settings1
and authenticated lock state. All model and service calls are confined to the
owning Qt thread; returned owning values may cross threads after a call returns.
Raw clipboard content must not appear in logs, diagnostics, board messages, or
repository tests beyond obviously synthetic fixtures.

Module-boundary and dependency-direction rules are in
[Module boundaries](module-boundaries.md). The durable decisions — volatile
history, allowlist storage, purge-on-privacy-loss with generation fencing,
and the pure-model seam ahead of the Wayland adapter — are recorded in
[ADR-0031](../adr/0031-volatile-bounded-clipboard-history.md) and the C1 process
boundary in [ADR-0056](../adr/0056-isolate-clipboard-capture-in-a-volatile-host.md).
