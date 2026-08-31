# S3 1440p125 shortcut diagnosis and repair-authority request

- Worker: Dorothy Vaughan
- Update: 2026-08-31T11:54:34-06:00
- Coordination head: `39c83a23143f36c3bfec8686b7cbc0121ea8a418`
- Accepted product ancestor: `48366af29f6f98c063483a16b2ad715556d14b44`
- Execution state: read-only diagnosis; compiler/CTest/private-bus/private-runtime
  lane remains released

## Material finding

Manager red `f9774759f885bb78f43b2b4e9fb97ef6` reached interaction
return 8 (`notification center did not map`), not the binding gate's return 9.
The current `queryDesktopNotificationBinding()` therefore accepted the exact
action plus default and active Meta+N lists before the sole target input batch.
Its implementation checks only `globalShortcutsByKey`, `defaultShortcut`, and
`shortcut`; it does not authenticate the component name, resolve the component
object, or require the component's `isActive` state.

KF6's installed primary interface distinguishes these states:

- `org.kde.KGlobalAccel` retains action/default/active registry metadata.
- `org.kde.kglobalaccel.Component.isActive` reports whether that component can
  currently receive activations; the API explicitly permits an inactive
  component to remain in the registry without triggering.
- `globalShortcutPressed`/`globalShortcutReleased` identify actual per-action
  activation, unlike binding-change metadata.

That gap exactly permits the observed result: metadata qualifies while the
single Meta+N is not dispatched to the live shell action. The red capture did
not record component activity or per-action activation, so this is the
smallest evidence-consistent causal hypothesis rather than a retrospective
claim that `isActive` was false.

The timing comparison is consistent and does not establish dock geometry as
the cause. Red probe-004 qualified with two mapped but zero-geometry docks and
then returned 8 after the three-second observation. Reviewer green
`da6ed5da9641bfe695a7c4e73a11b9e1` had no surfaces at probe-004, settled both
docks at probe-005, and delivered the same one-shot Meta+N. Both had the same
single WL-0 2048x1152 logical output and complete required service ownership.

## Proposed bounded repair

Within `tests/session/**` only:

1. Extend the binding record/query to authenticate the exact shell component
   (`qindaqt-shell`), resolve its component object, require a valid `isActive`
   reply, and require `true` immediately before the sole Meta+N batch.
2. Preserve one target input batch and no fixed delay or retry. Continue the
   bounded event-loop observation only for state publication.
3. Observe the exact component's press/release signal around that one batch so
   a future failure distinguishes “KGlobalAccel did not deliver the action”
   from “the delivered action did not map its surface.” This is diagnostic
   evidence, not a second target activation.
4. Add pure mutation coverage for inactive component, invalid/error state,
   wrong component, and existing wrong/missing/remapped bindings.

Expected touched files are
`tests/session/desktopnotificationbinding.{h,cpp}`,
`tests/session/desktopsessionprobe.cpp`, and
`tests/session/tst_desktopnotificationbinding.cpp`, plus only the smallest
needed CMake adjustment if a focused target changes. The testing contract also
requires a corresponding update to
`docs/wiki/development/testing-harness.md`.

I request explicit authority for those test/docs paths and, only after static
and owned units pass, reassignment of the serialized configure/build/runtime
lane. No product/profile/Network implementation edit is proposed.
