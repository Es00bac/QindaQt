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
  stale-generation rejection, explicit ownership/lifetime/error contracts,
  and value/descriptor codecs plus fixtures as the seam a future adapter
  composes. Static unit evidence only.
- **C1 (later slice, reviewed separately):** `qindaqt-clipboard-host` resident
  process, `org.qindaqt.Clipboard1` private authenticated bus surface,
  `ext-data-control-v1` adapter, Settings1 `services.clipboardHistory`
  opt-in wiring, lock-state provisioning, and presentation. C1 must not begin
  until the integrated Settings1, authenticated lock state, and verified KWin
  protocol support exist, per the platform-services lane plan.

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
never widen them; `isValidLimits` enforces that, and the constructor asserts
it.

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
non-storable, so a hostile value produces one deterministic error. Admission
also refuses empty values (no formats, or no format carrying any payload),
duplicate canonical media names, and oversized items.

## Privacy, opt-in, and generation fencing

The model starts fail-closed: history disabled and privacy `Denied`. Nothing
is admitted, nothing is disclosed, and snapshots return empty content with
the flags that explain why — not even aggregate byte totals are exposed while
withheld. The future host sets privacy `Allowed` only from authenticated
unlocked state, mirroring the notification presentation gate.

- Disabling the history or denying privacy **purges every entry and raises
  the generation by exactly one**. Re-stating the current authority value is
  a no-op.
- Every content mutation carries the caller's `expectedGeneration` and is
  refused with `StaleGeneration` on mismatch, so any decision made before a
  purge — including an in-flight admission — is rejected instead of touching
  post-purge state. Entry ids embed their generation, so pre-purge ids can
  never resolve afterwards even if serials repeat.
- Refusal order is fixed and tested: `HistoryDisabled`, `PrivacyDenied`,
  `StaleGeneration`, then value validation, then capacity.

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
  room the admission is refused with `CapacityRefused` and nothing changes —
  a refusal never evicts partially or mutates the history.
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
- **Revision:** increases by exactly one per successful state change;
  refusals and no-ops change nothing.

## Codecs and the adapter seam

Two canonical, versioned byte forms exist so the future C1 adapter never
invents its own serialization:

- **Value codec** (`QCBV` format 1): bounded inline payloads for round-trip
  and testing. A future transport may reuse it only within the protocol
  bounds; large transfers still move by FD and never through this form.
- **Descriptor codecs** (`QCBD` entry, `QCDL` list, format 1): metadata-only
  — identity, ticks, pins, sanitized source label, bounded preview, format
  names with byte counts, and the fingerprint. These are the intended basis
  of the C1 snapshot wire form, so presentation never writes its own
  serialization.

Both decoders share a hostile-input floor implemented once: little-endian
fixed framing, unknown version refusal, declared lengths trusted only when
the remaining buffer satisfies them, trailing bytes refused, canonical media
re-validated on decode, duplicates refused, and unknown flag bits refused so
future extensions cannot be silently misread by an older decoder.

## Boundaries

`clipboard_model` depends on Qt Core only. It contains no QObject, no IPC,
no Wayland, no persistence, and no clock. The C1 host will compose it with
transport and lock-state authority; C0 consumers (tests today, the C1 host
later) link `QindaQt::ClipboardModel`. Raw clipboard content must not appear
in logs, diagnostics, board messages, or repository tests beyond obviously
synthetic fixtures.

Module-boundary and dependency-direction rules are in
[Module boundaries](module-boundaries.md). The durable decisions — volatile
history, allowlist storage, purge-on-privacy-loss with generation fencing,
and the pure-model seam ahead of the Wayland adapter — are recorded in
[ADR-0028](../adr/0028-volatile-bounded-clipboard-history.md).
