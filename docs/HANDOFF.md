# Integration handoff

## Current baseline

- Branch: public `main`
- Functional commit: `1cd5dab`
- Tree: `ce5abd0717064bebad4a9ae2f1564a0897a7daa9`
- Outcome: qualified QST-1 and Controls, bounded Audio1, Display D0/D1, and
  executable native Text Editor S1
- State: independently accepted, manager-qualified, and published with a
  documentation-only project-identity descendant

The baseline combines generic persistent Settings1 and the first-class
Notifications route with QST-1's pure semantic token derivation, accessibility
overrides, read-only QML adapter, and installed consumer packages. The
Settings1 resident exits on permanent session-bus loss; a new daemon activates
a new process and lineage rather than reconnecting stale repository state.
QST-1 owns semantic policy without importing a general application framework
or widening the theme schema. Audio1 adds a versioned, asynchronous Qt
boundary over a resident service whose production WirePlumber and GLib handles
remain confined to one private worker thread. Run generations, owner/epoch/
revision lineage, and atomic validation prevent stale or malformed backend
state from reaching future shell and Settings consumers. Display D0/D1 adds a
revisioned compositor inventory plus bounded protocol, identity, topology, and
reversible transaction state. Text Editor S1 adds the first native application:
one local UTF-8 document with optimistic conflict checks and atomic persistence.

Integrated evidence:

- The exact Text Editor candidate passed independent review, then built in the
  integrated Debug tree and passed all 8/8 focused document, store, controller,
  large-document, offscreen window, desktop metadata, CLI, and installed-theme
  tests. Its accepted candidate also passed Release/package proof and measured
  266 ms startup with 19,511 KiB median PSS.
- The accepted Audio candidate and the exact integrated functional tree both
  received different-worker review with P1/P2/P3 `0/0/0`.
- Fresh strict-warning Debug and Release builds passed 749/749 steps each.
  The focused Audio selector passed 7/7 and the complete QindaQt registry
  passed 108/108 in both configurations.
- Debug and Release activation/runtime/reset lifecycle stress passed all three
  tests for ten repetitions each: 30 executions per configuration, 60 total.
- A fresh ASan+UBSan build passed 59/59 focused steps and all 7/7 Audio tests
  with leak detection and halt-on-error enabled, including the 250-cycle
  worker teardown and deterministic reset-source barriers.
- A fresh testing-disabled production/package build passed 485/485 steps and
  all four QML-lint targets. Its 186-file staged install contains the exact
  Audio executable, public libraries/headers, D-Bus descriptor and XML, and
  hardened systemd user unit with staged executable resolution.
- The exact installed Audio descriptor completed 10/10 private-D-Bus daemon-
  loss/replacement cycles: 20/20 staged service activations and exact PID exits,
  10/10 distinct owner/PID/epoch replacements, zero surviving staged services,
  and zero fixture roots.
- Documentation link/navigation validation, source-shape audit, strict MkDocs,
  whitespace, and post-test process cleanup passed on the integrated tree.
- No active desktop, user session bus, global input, host audio graph/device,
  physical display, or physical screen lock was touched by this evidence.

## Next outcome

Qualify the installed notification shortcut, keyboard/focus, Settings1
replacement, Do Not Disturb, shell restart, and real nested lock transitions
at 1080p, WUXGA, and 1440p as described in [Task list](TASK_LIST.md). That lane
must remain private and nested: it may not move the host cursor, inject host
input, replace the active compositor, alter host lock configuration, or use the
live session bus.

The reusable `QindaQt.Controls 1.0` component set is now integrated after exact
independent Debug/Release, visual, accessibility-event, package, source-policy,
and PSS qualification. The revisioned compositor output inventory and contained
virtual-output development seam are also integrated after exact review and
focused integrated-tree verification. The pure Display1 protocol, identity,
topology, and reversible transaction model are now integrated after the
same-revision lineage defect was reproduced, repaired, and exactly rereviewed.
In parallel with the virtual desktop outcome, implement the resident Display1
service/adapters. Power/Brightness architecture is independently accepted at
MODELLED only: PB-0 protocol/aggregation/model is the first ungated
implementation slice, followed by PB-1; PB-2 waits on the routed session-lane
activation contract. A source-only handoff or a live worker process is not
completion.
