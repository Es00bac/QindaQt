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
| `QindaQt::ShellClipboardApplet` (pure) | `ClipboardAppletModel`: deterministic, reentrant projection of one history snapshot plus client state into phase, bounded rows, format summaries, and complete accessible names/descriptions. Public ClipboardModel values plus Qt Core only. |
| `QindaQt::ShellClipboardAppletRuntime` | `ClipboardAppletController` (QML-facing facade), `ClipboardModelClientAdapter` (the injected seam over `ClipboardHistoryModel`), and the compiled QML module. Never service internals, transport, files, or persistence. |

`ClipboardClientInterface` is the injected least-authority seam: snapshots and
typed signals in, unique-id intent requests out. The controller borrows it;
shell composition owns the model and adapter lifecycle. QML receives no model
pointers, raw payloads, or IPC endpoints.

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
| `unavailable` | Owner lost, client unavailable, or read capability denied. |

Rows are bounded to `kMaxPresentedEntries` (32). Projection order is a stable
partition — every pinned entry first, then every unpinned entry, each class in
the snapshot's most-recent-first order; search-result rows use the same
partition. Every row carries a bounded preview with truncation flag, sanitized
source label, format summary, byte total, pin/pending state, and a complete
accessible name/description; pending mutations announce "operation pending".
Metadata only: payload bytes never leave the C0 model except through an
explicit promote, and the projection never holds them.

## Privacy, lock purge, and generation fencing

A lock is an authenticated authority denial, not a presentation hint:

- `ClipboardModelClientAdapter::setLocked(true)` denies model privacy *before*
  the lock becomes observable, so the model purges every entry and raises its
  generation by exactly one. Unlock restores only the authority the lock
  itself removed; an independent host denial survives unlock.
- The controller destroys its own presentation copy on the same signal:
  entries, byte totals, pending intents, feedback, and the entire search
  state.
- The generation bump fences the whole pre-lock lineage: pre-lock entry ids
  never resolve again, and unlock cannot redisclose pre-lock content.

Search reply freshness uses a controller-internal monotonically increasing
query generation. The seam promises request-id *uniqueness* only, never
ordering, so a reply is accepted only when its id maps to the query generation
that issued it; replies for superseded, abandoned, or replayed requests are
dropped regardless of numeric id. Seam signals emitted synchronously inside a
dispatch call are buffered and drained through one exact-id attribution path:
the current request and already-registered pending requests resolve, anything
else is discarded, so a hostile flush can neither impersonate the live request
nor strand an earlier one. Promote ticks are controller-issued monotonic
metadata, never wall clock.

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
fresh stage, builds a C++ consumer against only staged files, asserts the
relocated consumer's RPATH reaches the staged sibling modules without leaking
build/source-tree paths, and runs the lock/purge contract plus an offscreen
instantiation of the staged compiled module against the real controller.

## Focused tests

```sh
ctest --test-dir build/dev -R '^qindaqt\.clipboard-applet-' --output-on-failure
```

| Test | Scope |
| --- | --- |
| `qindaqt.clipboard-applet-model` | Pure projection: phases, fail-closed ordering, pinned-first partition, bounds, accessibility phrases, determinism. |
| `qindaqt.clipboard-applet-controller` | Generation/owner/lock fencing, read/write capability gates, pending bookkeeping, feedback, lineage exhaustion. |
| `qindaqt.clipboard-applet-fencing` | Hostile-seam attribution: unique-but-unordered ids, superseded-reply flushes inside dispatch calls, injected/duplicated completions, cross-request synchronous drain, monotonic promote ticks. |
| `qindaqt.clipboard-applet-seam` | Adapter lock-as-privacy-denial ordering, error mapping, owner fencing over the real C0 model. |
| `qindaqt.clipboard-applet-qml-offscreen` | Compiled module states: ready/degraded/locked/disabled/unavailable/empty/search presentation. |
| `qindaqt.clipboard-applet-qml-accessibility-offscreen` | Accessible roles, names, descriptions, and state. |
| `qindaqt.clipboard-applet-qml-keyboard-offscreen` | Keyboard traversal and activation paths. |
| `qindaqt.clipboard-applet-qml-interactive-offscreen` | Real pointer events reach Pin/Delete/row body (P1 regression), degraded-state honesty, busy pending controls. |
| `qindaqt.clipboard-applet-boundary-policy` | Static source gate with eight per-case poison probes (D-Bus, host clipboard, external helpers, private model headers, compositor reach-through). |
| `qindaqt.clipboard-applet-installed-package` | Staged component artifacts, relocated-consumer RPATH poison check, lock/purge contract and staged-module instantiation at the installed boundary. |

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
