# Clipboard applet

`src/shell/clipboard_applet` owns the compiled presentation and controller
surface for the production panel Clipboard applet: a pure projection of the
[Clipboard service](../architecture/clipboard-service.md) C0 volatile bounded
history, a QObject controller over an injected least-authority client seam, an
in-process adapter over the C0 model, and the compiled
`QindaQt.Shell.ClipboardApplet 1.0` QML module. The applet never contacts
Wayland data devices, X11 selections, host clipboard engines, D-Bus, or
clipboard-model internals; every byte of content authority stays behind the
C0 model's fail-closed gates. The authority split is recorded in
[ADR-0031](../adr/0031-volatile-bounded-clipboard-history.md) and the applet
resolution rules in [Applet runtime](applet-runtime.md).

Current maturity: **registered built-in presentation slice (compiled and
verified)**. The manifest (`data/applets/clipboard.json`), audited registry
entry (`qindaqt.applets.clipboard`), and explicit `clipboard.read` /
`clipboard.write` policy grants are integrated, so the applet host resolves a
profile instance to `ready`. Composition into the production shell dispatcher
(`src/shell/runtime`, `src/shell/qml`) is a separate lane after integration;
the stock profile does not place the applet yet.

## Module shape

Two static targets mirror the Power applet's split (the read-only C0 model
ships as a non-PIC static archive, so no shared applet library can embed it):

| Target | Responsibility |
| --- | --- |
| `QindaQt::ShellClipboardApplet` (pure) | `ClipboardAppletModel` (deterministic, reentrant projection of one history snapshot plus client state into phase, bounded rows, format summaries, and complete accessible names/descriptions) and the snapshot admission gate (`clipboard_snapshot_gate.h`): the hostile-input floor every incoming descriptor collection must pass. Public ClipboardModel values plus Qt Core only. |
| `QindaQt::ShellClipboardAppletRuntime` | `ClipboardAppletController` (QML-facing facade), `ClipboardModelClientAdapter` (the injected seam over `ClipboardHistoryModel`), and the compiled QML module. Never service internals, transport, files, or persistence. |

`ClipboardClientInterface` is the injected least-authority seam: snapshots and
typed signals in, unique-id intent requests out. The seam is GUI-thread
confined (cross-thread backends marshal through queued connections), the
controller borrows it and never deletes it, request ids are unique but
unordered with zero a valid id, and completions must carry the entry lineage
they resolve — a completion whose id disagrees with the recorded request is
rejected whole. The full threading/lifetime/error contract is stated in the
interface header. The controller borrows it; shell composition owns the model
and adapter lifecycle. QML receives no model pointers, raw payloads, or IPC
endpoints.

## Capability gating

The manifest requests `clipboard.read` and `clipboard.write` independently;
`data/applet-policy/default.json` grants both to the audited `clipboard`
package only. The composing shell passes the evaluated grants to the
controller at construction (fail-closed, immutable):

- `clipboard.read` denied: observation is withheld entirely — the phase
  reports unavailable (`clipboard-read-not-granted`), no entries are retained,
  and search never reaches the seam.
- `clipboard.write` denied: browsing and search stay live, but every mutating
  intent (select/promote, pin, delete, clear) is refused with feedback before
  any dispatch.

## Presentation contract

Phases, exposed as `phaseText` with a fixed `phaseReasonText`:

| Phase | Meaning |
| --- | --- |
| `loading` | Client initializing or waiting for the initial snapshot. |
| `ready` | Owner available, history enabled, privacy allowed. Rows and controls active. |
| `degraded` | Owner available but limited: read-only browsing; every mutating control is visibly disabled, and the controller refuses mutations anyway. |
| `locked` | Session locked **or** privacy denied (distinct registered reason texts; no seventh phase). Content withheld. |
| `disabled` | History disabled by user setting. |
| `unavailable` | Owner lost, client unavailable, read capability denied, or a refused incoming snapshot (`invalid-snapshot`). |

Rows are bounded to `kMaxPresentedEntries` (32). Projection order is a stable
partition — every pinned entry first, then every unpinned entry, each class in
the snapshot's most-recent-first order; search-result rows use the same
partition. Every row carries a bounded preview with truncation flag, sanitized
source label, format summary, byte total, pin/pending state, and a complete
accessible name/description; pending mutations announce "operation pending".
Metadata only: payload bytes never leave the C0 model except through an
explicit promote, and the projection never holds them.

## Snapshot admission gate

Everything arriving through the seam — every history snapshot and every
search-reply match list — passes a hostile-input admission gate
(`clipboard_snapshot_gate.h`, reused by both the controller and the pure
projector as defense in depth) **before** any descriptor reaches the
projection or retained controller state:

- **Descriptor floor.** Each entry must satisfy the public C0 descriptor
  floor, reused through the canonical descriptor codec (never restated): valid
  generation-tagged identity, nonempty bounded canonical format list with
  unique names, non-negative bounded claimed bytes, sanitized label/preview
  (no control or bidi format characters, no unpaired surrogates, bounded
  length), consistent truncation flag, exact fingerprint width.
- **Media allowlist.** Entries carrying sensitive, one-time, or non-storable
  media classes are forged, not unusual — the C0 model never stores them.
- **Collection and aggregate bounds.** A snapshot above the C0 `kMaxEntries`
  ceiling or with a negative/oversized aggregate byte claim is refused whole;
  the claim must also equal the sum of every descriptor's format-byte claims.
  Entry identities are unique and no more than C0's eight pinned entries may
  appear. The 32-row presentation cap is never the thing that saves us.
- **Authority consistency.** Generation zero is never C0 truth. Whenever
  history is disabled or privacy is denied, both the descriptor list and
  aggregate byte claim must be empty. These contradictions have distinct typed
  `SnapshotGateDecision` refusals rather than becoming retained hidden state.
  Withdrawing either authority must also advance generation from the last
  accepted snapshot, matching C0's mandatory purge fence.
- **Lineage monotonicity.** Accepted (generation, revision) is a high-water
  mark: anything below it is stale or replayed and refused; re-stating the
  exact accepted lineage is an idempotent re-delivery. Entries whose id
  generation disagrees with the snapshot's own generation are refused (the C0
  model purges on generation change, so mixed lineage cannot be legitimate).
- **Owner lineage.** Content is accepted only under the owner recorded with
  the baseline. Owner loss or replacement voids the whole baseline — content,
  high-water fences, pending intents, search state — and the next snapshot
  under a replacement owner must be content-empty, because volatile history
  starts empty per owner; non-empty content there is the previous owner's
  history replayed through the new owner. No baseline is established from a
  snapshot delivered while the client reports the service unavailable (the
  adapter's content-empty fallback in that state would otherwise arm the next
  real snapshot).

Any violation fails closed: the presented copy, pending intents, and search
state are destroyed and the surface reports unavailable (`invalid-snapshot`)
until a fresh valid snapshot arrives. The accepted high-water fence survives
the rejection, and a structurally impossible authority snapshot poisons its
generation so changing a flag or revision cannot arm rejected content;
recovery requires a later valid generation or a fresh owner baseline. Other
structural refusals poison their exact lineage until a later valid snapshot.
Rejection is not a permanent latch.

## Privacy, lock purge, and generation fencing

A lock is an authenticated authority denial, not a presentation hint:

- `ClipboardModelClientAdapter::setLocked(true)` denies model privacy *before*
  the lock becomes observable, so the model purges every entry and raises its
  generation by exactly one. The adapter tracks the lock-derived denial
  separately from an independent host denial delivered through
  `setHostPrivacyDenied()`: privacy stays denied while *either* cause is
  active, so unlock never overrides a host denial — including one that arrived
  while the lock denial was active — and a host re-allow while locked never
  bypasses the lock. A denial already present at lock time that neither cause
  explains is recorded as foreign and likewise survives unlock.
- The controller destroys its own presentation copy on the same signal:
  entries, byte totals, pending intents, feedback, and the entire search
  state.
- The generation bump fences the whole pre-lock lineage: pre-lock entry ids
  never resolve again, and unlock cannot redisclose pre-lock content.

Search reply freshness uses a controller-internal monotonically increasing
query generation plus the complete snapshot lineage at dispatch: generation,
revision, and exact descriptor membership. Every accepted snapshot change
immediately clears prior results and abandons or reissues the query, including
same-generation revision changes; an accepted reply must still contain only
descriptors that are exact members of the current accepted snapshot. The seam
promises request-id *uniqueness* only, never ordering, so a reply is accepted
only when its id maps to the query generation and snapshot lineage that issued
it; replies for superseded, abandoned, or replayed requests are dropped
regardless of numeric id. Seam signals emitted synchronously inside a dispatch
call are buffered and drained through one exact-id attribution path:
the current request and already-registered pending requests resolve, anything
else is discarded, so a hostile flush can neither impersonate the live request
nor strand an earlier one. Operation completions clear the pending marker
through the **stored request's** entry id, never the completion's id field:
entry-operation completions require a valid exact entry id, while Clear
requires no entry id. Missing, foreign, or unexpected lineage is rejected
whole, so it can neither unpin another entry's marker nor forge feedback.
Promote ticks are controller-issued monotonic metadata, never
wall clock, raised above every tick observed in a snapshot; the fixed-width
counter fails closed at exhaustion — the promote is refused with feedback
rather than issuing a wrapped (non-monotonic) tick.

## Intents

`selectEntry` (promote to the live selection), `deleteEntry`, `togglePin`,
`clearHistory(unpinnedOnly)`, and `setSearchQuery`/`clearSearch` are intents
dispatched through the seam — the controller never executes against model
state directly. Every mutation enforces generation fencing locally before
dispatch: a stale target generation is refused fail-closed with user feedback.
In-flight entries carry a pending marker that blocks duplicate intents and
renders busy controls.

## Packaging

The `ClipboardApplet` install component packages the public boundary: the
static backing archives and generated plugin archive beside `qmldir`,
`.qmltypes`, and the QML files under `QindaQt/Shell/ClipboardApplet` in the Qt
QML tree; the applet's public headers under the include directory; and the
manifest under `qindaqt/applets`.
`qindaqt.clipboard-applet-installed-package` installs the component into a
fresh stage, builds a C++ consumer against only staged files, then proves
genuine relocation: every staged Controls/Tokens backing library and optional
QML plugin has its build-tree RUNPATH rewritten to `$ORIGIN`-relative entries
(`patchelf`), the consumer resolves its stage root from its own executable
location, and after a passing run at the original prefix the whole stage is
**moved** and the consumer rerun with `LD_LIBRARY_PATH` unset. The `readelf`
assertions enumerate every staged dynamic artifact, require an
`$ORIGIN`-relative RPATH, and reject any absolute stage/build/source-tree path.
The row
also runs the lock/purge contract plus an offscreen instantiation of the
staged compiled module against the real controller at both locations.

## Focused tests

```sh
ctest --test-dir build/dev -R '^qindaqt\.clipboard-applet-' --output-on-failure
```

| Test | Scope |
| --- | --- |
| `qindaqt.clipboard-applet-model` | Pure projection: phases, fail-closed ordering, pinned-first partition, bounds, accessibility phrases, determinism, and exact whole-projection rejection of floor/bound/hostile-match violations. |
| `qindaqt.clipboard-applet-controller` | Generation/owner/lock fencing with presented owner-A content, read/write capability gates, pending bookkeeping, feedback, lineage exhaustion. |
| `qindaqt.clipboard-applet-fencing` | Hostile-seam attribution: unique-but-unordered ids, complete generation/revision/entry-set search fencing, superseded-reply flushes inside dispatch calls, injected/duplicated completions, cross-request synchronous drain, monotonic promote ticks. |
| `qindaqt.clipboard-applet-admission` | Snapshot admission: descriptor floor, media allowlist, collection/aggregate bounds, (generation, revision) high-water, owner-lineage fencing with owner-A content, fail-closed rejection and recovery, missing/mismatched completion-lineage rejection, promote-tick exhaustion. |
| `qindaqt.clipboard-applet-snapshot-invariants` | Whole C0 snapshot truth: nonzero generation, denied/disabled emptiness, exact aggregate sum, unique identities, pin ceiling, and denied-content lineage poisoning. |
| `qindaqt.clipboard-applet-seam` | Adapter lock-as-privacy-denial ordering, independent and overlapping host denials surviving unlock, error mapping, owner fencing over the real C0 model. |
| `qindaqt.clipboard-applet-qml-offscreen` | Compiled module states: ready/degraded/locked/disabled/unavailable/empty/search presentation. |
| `qindaqt.clipboard-applet-qml-accessibility-offscreen` | Accessible roles, names, descriptions, and enabled/busy state for every interactive element: search field and clear, Pin, Delete, both Clear buttons, feedback dismissal (alert role). |
| `qindaqt.clipboard-applet-qml-keyboard-offscreen` | Real Tab/Backtab traversal across every interactive element and Space/Return/Delete keyboard activation with exact intent arguments. |
| `qindaqt.clipboard-applet-qml-interactive-offscreen` | Real pointer events reach Pin/Delete/row body with exact `(generation, serial)` arguments (P1 regressions), read-only search stays enabled under write denial, degraded-state honesty, busy pending controls. |
| `qindaqt.clipboard-applet-boundary-policy` | Static source gate with eight per-case poison probes (D-Bus, host clipboard, external helpers, private model headers, compositor reach-through). |
| `qindaqt.clipboard-applet-installed-package` | Staged component artifacts, exhaustive backing/plugin/consumer RUNPATH inspection, genuine stage relocation with `LD_LIBRARY_PATH` unset and `$ORIGIN`-relative RPATH, lock/purge contract and staged-module instantiation at the installed boundary. |

The boundary gate also runs without configure:

```sh
cmake -DQINDAQT_CLIPBOARD_APPLET_SOURCE_DIR=<repository>/src/shell/clipboard_applet \
  -DQINDAQT_CLIPBOARD_APPLET_POISON_DIRECTORY=<scratch> \
  -P tests/shell/clipboard_applet/check_clipboard_applet_boundary.cmake
```

## Non-claims

This slice proves no Wayland `ext-data-control-v1` transport, no
`org.qindaqt.Clipboard1` bus surface, no live host-clipboard integration, no
Settings1 opt-in wiring, and no production-shell composition; the adapter is
an in-process seam over the C0 model for presentation and tests. The C1 host
process, authenticated lock-state provisioning, and live transport remain the
platform lane's milestones, per
[Clipboard service](../architecture/clipboard-service.md).
