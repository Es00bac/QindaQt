# Pavel Shore — Recovery claim: Display1 D3 client and reversible-transaction coordinator

**Context:** Nyra Sol's Moonshot Kimi process ended on a verified account-wide
weekly-limit HTTP 403 mid-implementation. Her partial delivery is fully
preserved; nothing is discarded or reformatted. I am the accountable recovery
lead for the complete outcome and final commit.

**Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/display-d3-kimi-nyra`
**Branch:** `worker/display-d3-kimi-nyra`
**Base commit:** `146fc48358c2659436dec4fc6b6062d23c5ee746`
**Provider/model:** Anthropic Claude Code, `claude-sonnet-5`, reasoning high.

## Nyra's preserved partial tree (unmodified, verified byte-for-byte)

- `src/CMakeLists.txt` — additive `add_subdirectory(services/display_client)` line only.
- `src/services/display_client/CMakeLists.txt` — module target, links
  `QindaQt::DisplayProtocol`, `QindaQt::DisplayTopology`, `Qt6::Core`,
  `Qt6::DBus`; registers `display_client.h`, `display_coordinator.h` (not yet
  authored), and three `.cpp` sources.
- `include/qindaqt/services/display_client/display_transport.h` — complete
  abstract `DisplayTransport` boundary (owner/invalidation/reply signals).
- `include/qindaqt/services/display_client/qt_display_transport.h` +
  `src/qt_display_transport.cpp` — complete production D-Bus transport:
  owner-generation-fenced `GetNameOwner`/`ServiceOwnerChanged` tracking,
  normalized error codes, async `GetSnapshot`/`Stage`/`Preview`/`Confirm`/`Cancel`.
- `include/qindaqt/services/display_client/display_client.h` — complete
  declaration of the `Client` QObject: `ClientState` machine
  (Stopped/Starting/Ready/Unavailable/Degraded/Busy), epoch/revision-fenced
  `PendingOperation` tracking, `stage/preview/confirm/cancel`, and
  `stateChanged`/`snapshotChanged`/`operationCompleted` signals.

**Missing and being implemented by me:** `src/qt_display_client.cpp` — I mean
`display_client.cpp` (the `Client` implementation) and
`display_coordinator.h`/`.cpp` (not started at all — no header exists yet).

## Tara Wells's boundary (read, not edited)

- Exclusive: `tests/services/display_client/**`. Her record
  (`ops/team/workers/tara-wells.md`) shows 5 hostile QtTest binaries
  (owner lineage A/B/A, snapshot atomicity, operation state machine,
  timeout/late-reply, service state transitions) plus support fixtures,
  already registered in `tests/CMakeLists.txt`, all blocked awaiting
  `display_client.cpp`/`display_coordinator.cpp`.
- **Material finding for Tara:** every one of her 5 test files includes
  `<qindaqt/services/display_client/client.h>`, but Nyra's actual header is
  `display_client.h`. I'm resolving this from my owned side by renaming the
  header to `client.h` (matching what's already written against, zero edits
  to your files) rather than asking you to change tests. Will confirm once
  landed.

## Coordinator design decision

`display-service.md` states D3's mandate is "a typed async client"; revert is
a server-side D1/D2 transaction-machine concept, not a client action. I'm
keeping `Client` as the thin async RPC/state binder Nyra already designed, and
adding `Coordinator` as a genuinely distinct caller-facing layer (not a
pass-through duplicate): it owns single-transaction sequencing
(stage→preview→confirm|cancel), fail-closed gating on `Client` state/epoch,
and a client-side bounded confirmation deadline that auto-cancels
(defense-in-depth revert trigger) if a caller never decides — matching the
"fail-closed reversible preview/confirm/revert coordinator" requirement
without duplicating `Client`'s protocol plumbing.

**Acceptance target:** unchanged from Nyra's claim — strict-warning
Debug/Release, hostile fake-transport coverage, installed public-header
consumer, source-shape/docs/link/MkDocs/whitespace/clean-tree gates, one
clean milestone commit, non-Claude exact review requested at handoff.

**Collision/dependency risks:** None beyond the header-name fix above. No
concurrent Display worker is active on the board.
