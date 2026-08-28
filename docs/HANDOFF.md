# Integration handoff

## Current baseline

- Branch: public `main`
- Functional boundary: public `9db68c4` plus accepted AppShell candidate
  `5c914a6` in this integration change
- Outcome: qualified QST-1 and Controls, bounded Audio1, Display D0/D1/D2,
  live notifications, executable native Text Editor S1, and executable shared
  QindaQt.AppShell 1.0 contracts
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
The installed Notification Live path qualifies the shell shortcut,
keyboard/focus behavior, Settings1 persistence and replacement, Do Not Disturb,
critical bypass, shell restart, authenticated private lock privacy, and bounded
teardown across the required nested resolution and scale matrix.

Integrated evidence:

- The exact repaired AppShell candidate `5c914a6` passed independent GLM
  rereview with no blocking finding. The combined tree then built the five
  AppShell targets serially and passed 5/5 action-registry, coordinator,
  offscreen accessibility/close-consent, source-policy, and clean installed-
  consumer rows, plus documentation navigation, strict MkDocs, source shape,
  and whitespace checks.
- The immutable Notification Live candidate passed an independent five-profile
  private nested matrix and ten repeated 1080p lifecycles. The conflict-resolved
  manager commit then passed an independent exact-tree integration review, a
  fresh 1,299-action combined Debug build, 11/11 exact focused regressions, and
  a fresh installed private 1080p smoke. No matching private process or recent
  fixture root remained afterward.
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

Complete the interactive virtual desktop integration described in
[Task list](TASK_LIST.md): boot the combined compositor, shell, resident
services, and test applications under an isolated parent Wayland compositor;
exercise the private nested seat; collect reviewable screenshots; and prove
repeatable teardown across the resolution, scale, theme, and multi-output
matrix without connecting to the host pointer, display, session bus, or user
configuration.

The reusable `QindaQt.Controls 1.0` component set is now integrated after exact
independent Debug/Release, visual, accessibility-event, package, source-policy,
and PSS qualification. The revisioned compositor output inventory and contained
virtual-output development seam are also integrated after exact review and
focused integrated-tree verification. The pure Display1 protocol, identity,
topology, and reversible transaction model are now integrated after the
same-revision lineage defect was reproduced, repaired, and exactly rereviewed.
The resident Display1 service and exact-owner compositor inventory adapter are
now integrated at `a5528f8` after the A/B/A epoch-reuse defect was repaired and
two independent exact reviews passed. A fresh combined-tree Debug build passed
68/68 focused build steps and all five Display1 service tests, including both
serial private-D-Bus lifecycle rows, with no surviving service or fixture.
Display1 remains fail-closed for output mutation: D3 client, writer binding,
persistence, Settings UI, and nested preview/confirm/revert proof are next.
Power/Brightness architecture is independently accepted at MODELLED only:
PB-0 protocol/aggregation/model is the first ungated implementation slice,
followed by PB-1; PB-2 waits on the routed session-lane activation contract. A
source-only handoff or a live worker process is not completion.
