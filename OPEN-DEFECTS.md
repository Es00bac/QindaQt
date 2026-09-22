# Open defects

Findings from the live multi-output session of 2026-09-21/22. Ordered by how
likely you are to walk into them.

Evidence throughout is from `~/.local/share/sddm/wayland-session.log`. **That
file is the whole session's stderr, not the shell's alone** — see item #3 for a
defect that was filed purely because a warning from another process in that log
was attributed to the shell. Later items add evidence taken directly from the
running compositor over D-Bus and from the Wayland wire, which does not have
that ambiguity.

## Status

| | |
| --- | --- |
| Fixed on this branch | #1, #2, #6, #8, #11, #12, #13, half of #14 |
| Withdrawn, not a defect | #3 |
| Decided + diagnosed, symptom unchanged | #4 (ADR-0237) |
| Still open | #0, #5, #7, #9, #10, rest of #14, #15 |

Branch head is `06e2370c`. **The installed package is
`gui-wm/qindaqt-desktop-0.1.0_pre20260921-r10`, which pins `fcce2203`.**
Everything marked fixed is fixed in git and *not* on the running desktop until
a package cut and a shell restart. Nothing below was validated against a
rebuilt live shell.

New ADRs: [0235](docs/wiki/adr/0235-escrow-panels-whose-output-is-absent.md),
[0236](docs/wiki/adr/0236-fade-the-scene-root-not-the-panel-window.md),
[0237](docs/wiki/adr/0237-the-visibility-snapshot-stays-atomic.md).

---

## 0. A container stranded by a removed output wedges the whole snapshot — FIXED (`(pending)`)

**You will see:** no panel ever hides, on any display, for the rest of the
session. This is the live state of the running desktop as of 2026-09-22 01:2x.

This is not the transient flapping item #4 describes. The compositor is
returning a permanently invalid snapshot:

```console
$ busctl --user call org.qindaqt.Compositor /org/qindaqt/Compositor \
    org.qindaqt.Compositor1 ShellVisibilitySnapshot
{"epoch":"6b22d120-...","failure":{"code":"snapshot-invalid",
 "message":"a managed window is invalid or ambiguous"},
 "revision":"6479","schemaVersion":1,"status":"unavailable"}
```

Repeated over a minute, it never recovers. The session log holds **8250** of
that message. With no valid snapshot the shell selects safe-visible and pins
every panel visible.

**Why it cannot recover.** `Outputs` reports one output at generation 46:

```
eDP-1  1920x1080 @ (0,0)  scale 1  enabled
```

`Windows` reports 14 managed windows, and two of them are nowhere near it:

```
71fffc59  1163x1016 at (1,1133)
fcc21a4d   751x1016 at (1166,1133)
```

y=1133 is below the bottom of a 1080-tall output. Every snapshot candidate
therefore contains a window whose frame lies entirely outside its own output,
which is validation condition 5, which rejects the whole batch — every time,
forever.

**Inference, not observation:** the rejected candidate is not published, so
condition 5 is deduced from the geometry above rather than read from a log. The
messages landing in #4 are what would confirm it per occurrence.

**The windows are a symptom. A *container* was stranded.** Both belong to one
Hybrid container, whose own plan is off-screen:

```
Compositor1.Windows      71fffc59  containerId=hybrid-r133-container  (1,1133) 1163x1016
                         fcc21a4d  containerId=hybrid-r133-container  (1166,1133) 751x1016
Compositor1.Containers   hybrid-r133-container  authority=hybrid-process
                         outerFrame    (0,1104) 1918x1046
                         outerTitleBar (1,1105)
```

The other two containers are fine at (0,32). So the defect is that **nothing
relocates container plans when an output is removed**, and the stranded members
follow from that.

**Moving the windows does not work, and this was tested rather than assumed.**
A KWin script that relocates any window lying entirely outside the work area
ran and KWin performed the assignment:

```
js: kwin-rescue: work area 1920x1048 at (0,32)
js: kwin-rescue: relocating '• Untitled — QindaNote' from (1,1133) 1163x1016 -> (378,48)
```

`Compositor1.Windows` still reported (1,1133) afterwards and the snapshot stayed
`unavailable`. This is documented behaviour, in
[Panel visibility policy](docs/wiki/shell/panel-visibility.md): "the Hybrid
compositor's queued reconciliation restores only owned members to their
container-planned frames". A member cannot be rescued at the KWin level while
its container plan still says y=1104. Anything that fixes this has to move the
**container plan**, which is owned by `hybrid-process`.

**This is the evidence ADR-0237 said it was waiting for**, and it points the
opposite way from the item #4 write-up: the invalid window is not transient, it
is *stuck*. It also decided between the two candidate fixes. Excluding an
off-output window from the snapshot would have restored auto-hide while leaving
the user a container they cannot reach, so the fix is the other one.

**Fixed by relocating the container plan.**
`KWinHybridSession::reconcileWorkAreaGeometry()` already ran on every work-area
transition, but it only called `refreshMaximizedAreas()`, which re-fits
*maximized* containers alone. An ordinary container stranded by a display going
away was never relocated — and the grouped geometry reconciler immediately
after it reasserts each container's committed target frames, which is precisely
why a manual KWin-level move of a member was undone.

`HybridContainerPlacementController::refreshStrandedContainers()` now runs in
the same transition, **before** the grouped reconciler, and brings back any
container whose frame lies entirely outside its work area. It moves rather than
resizes, so a user's layout survives the display change, and only clamps when
the container no longer fits. A container that merely hangs off an edge is left
alone — that is the user's own arrangement, not a stranding.

Covered by `compositor.hybrid-container-placement`
(`reflowsContainersStrandedOffEveryOutput`), which uses the geometry measured
off the running session: work area (0,32) 1920x1048, container at (0,1104)
1918x1046. It also pins that a reachable container is untouched, that the pass
is idempotent, that an oversized container is clamped, that a maximized one is
left to `refreshMaximizedAreas()`, and that a reflow failure is reported.

**Not verified live.** The wiring into `reconcileWorkAreaGeometry()` needs a
running KWin; only the controller is unit-testable. The running session is
still wedged until a package cut.

---

## 1. Meta+right-click dies whenever a pinned display is absent — FIXED (`61248193`)

Customization stopped entirely — no panel, applet, or desktop menu — whenever a
panel was pinned to a display that was unplugged or being reconfigured:

```
QindaQt shell live customization is unavailable: panel 'dock' names missing output 'eDP-1'
```

`LayoutEditingRepository` failed its initial solve, so it initialized non-ready
and every coordinator command was refused. Both editor compositions sit on that
repository, so this also closed the Settings Customize route.

**The fix originally proposed in this file was dangerous — do not use it.**
Filtering through `PanelProfileOutputOccupancy::presentOutputsOnly()` before
building the editor host is safe for the *surface* path, where the filtered
profile is never written back, but the editor **persists what it holds**. An
Apply would have written the filtered profile to the user store and permanently
deleted every panel pinned to a display that happened to be absent. Silent data
loss, on the ordinary path of customizing a panel with the lid shut.

Fixed by escrow instead: the repository parts absent-output panels from the
session at construction and re-attaches them at the persistence boundary, so
they survive a round trip through Apply and return when their display does.
Candidate validation never extends the escrow, so an *edit* naming a
disconnected display is still refused. See
[ADR-0235](docs/wiki/adr/0235-escrow-panels-whose-output-is-absent.md).

---

## 2. The panel hide/reveal fade does nothing — FIXED (`d1394485`)

Panels *popped* in and out instead of fading, and the configured motion
duration had no visual effect. `QtPanelVisibilityAnimation::animate()` animated
`QWindow::opacity`, which Qt's Wayland platform does not implement — 662
warnings in one session.

Measured against the session compositor on Qt 6.11.1: `setOpacity()` on a
wayland `QQuickWindow` warns and changes no pixels, while setting `opacity` on
`contentItem()` changes them as expected (white over black at 0.25 grabs as
`#404040`). The fade now animates the scene root.

The port also gained `restore()`, because with the fade on a different object
the producer's four `setOpacity(1.0)` resets would have stranded panels
part-transparent. See
[ADR-0236](docs/wiki/adr/0236-fade-the-scene-root-not-the-panel-window.md).

---

## 3. ~~The dock's input mask is a no-op~~ — WITHDRAWN, not a defect

**This entry was wrong.** `QWindow::setMask()` *is* implemented on Qt Wayland
6.11.1 and the panel input region is set correctly.

```console
$ nm -DC /usr/lib64/libQt6WaylandClient.so.6 | grep QWaylandWindow::setMask
0000000000090900 T QtWaylandClient::QWaylandWindow::setMask(QRegion const&)
```

Confirmed on the wire with `WAYLAND_DEBUG=1` against the live compositor: the
needle rect reaches `wl_surface.set_input_region` in both orders, because Qt
replays a mask stored before show at platform-window creation. The
`AGENT-GUARD` in `runtimepanelwindowfactory.cpp` is accurate, and an
`AGENT-NOTE` there now records this so it is not "fixed" again.

**Where the error came from, because it can repeat:** the session log is the
whole *session's* stderr. The `does not support setting window masks` lines
come from another Qt client in that session (`PlasmaQuick::Dialog`, which masks
on X11). The neighbouring item #2 survives the same scrutiny, and the contrast
is what makes the mistake legible — `QWaylandWindow` overrides `setMask` but
has **no** `setOpacity` override, so of two near-identical warnings only the
opacity one can be the shell's. Attribute a log line to a process before you
attribute a defect to code.

---

## 4. One transient window kills the whole visibility snapshot — DECIDED (`2b39e7e8`)

Rejection is all-or-nothing: one invalid member voids the batch, and the shell
pins every panel visible until a clean snapshot arrives.

**The decision (ADR-0237): the batch stays atomic.** The wiki states the
contract as "One bad member rejects the complete batch; partial visibility
publication is not a supported state", and that second clause settles it —
dropping members silently changes what "the set of managed windows" means for
every consumer.

**What was actually actionable was the diagnosis.** Eight conditions shared one
message, so a rejection said only that *something* was wrong with *some*
window. Two of them are transient races (a frame outside its output; a window
active while marked minimized), and five mean the producer sent something
structurally wrong. Excluding is right for the first group and wrong for the
second, and the log could not tell them apart across 8250 occurrences.

Every rejection now names the member and the cause — eight window conditions,
the duplicate-active check, and four output conditions, each distinct and
carrying the identifier and offending geometry.

**This does not reduce the rejection rate.** It makes the next session's log
say which of thirteen causes produced it. See item #0: the answer on the
running desktop appears to be condition 5, persistently.

---

## 5. The shell leaks per display-hotplug — OPEN, and the evidence now conflicts

The original measurement, on the pre-fix shell while `outputGeneration` climbed
9 → 15:

| | start | +28 min | +1 h |
| --- | --- | --- | --- |
| threads | 78 | 91 | 114 |
| RSS | 289 MB | 420 MB | 490 MB |

**Counter-observation on the running r10 shell.** Sampled twice in one session:

| | at 3895 s | at 6662 s |
| --- | --- | --- |
| threads | 97 | **50** |
| RSS | 504 MB | **386 MB** |

Both went *down*, across an output-generation change that removed `DP-1`
(gen 45 → 46). That is not what a monotonic per-hotplug leak predicts. It is
consistent with resources scaling with the number of live panel windows — which
halved when the second output went away — rather than accumulating.

That does not clear the item: the original figures were taken while generations
were being *added*, and both readings here are single samples. But it does mean
the "roughly three GL contexts leak per output-generation change" reading is
not established.

**Next step is unchanged and now better motivated:** republish the panel set N
times in a controlled nested session at a *fixed* output count and count
`/proc/<pid>/task`. Varying the output count confounds the measurement.

---

## 6. `WindowsChanged` fires ~20 Hz on an idle desktop — NOT REPRODUCIBLE (`6e4708a5`)

Original: **602 signals in 30 s** on an idle desktop. Re-measured the same way
on r10: **1 in 30 s, 0 in 60 s**, with shell and KWin both at 0% of a core
sampled from `/proc`. The original was taken while an orphaned
`qindaqt-xembed-tray-proxy` was spinning a full core, which is the likely
source.

The underlying sloppiness was real and is fixed anyway: `ManagedWindowRegistry`
emitted `windowsChanged` synchronously per `frameGeometryChanged` and
`captionChanged` — one D-Bus broadcast per frame of any drag — and is now
coalesced at the source to one emission per event-loop turn.

Treat the 20 Hz figure as an artefact of the pre-fix machine state.

---

## 7. Display mirroring fails — OPEN, needs a live retry first

**Not diagnosed, and do not treat it as a code defect yet.** The `Display1`
service was **hung for the entire period of the attempts** — D-Bus introspect
timed out while the unit read `active` — and was restarted at 23:14. Every
mirror attempt before that was doomed regardless of configuration.

What is known:

- Mirroring is implemented, not missing — draft (`setOutputMirror`,
  `display_settings_draft.cpp:210`), topology validation (`UnknownMirrorSource`,
  `MirrorSelfReference`, `MirrorCycle`), and a Display1
  stage/preview/confirm transaction.
- The draft-level logic does no mode or scale checking, so any rejection is at
  the Display1 service or the compositor.
- The `DisplayMirrorRow.qml` `TypeError`s are real (item #8) and did disable
  the mirror control, but they predate the recent attempts.

**A mirror configuration may have partially applied.** The last *valid*
snapshot before the current wedge (generation 45) described both outputs at
identical geometry, with the internal panel scaled to match:

```
DP-1    1920x1200 @ (0,0)  scale 1
eDP-1   1920x1200 @ (0,0)  scale 0.9
```

`eDP-1`'s real mode is 1920x1080, and 1080 ÷ 0.9 = 1200 — the compositor was
scaling the internal panel to carry a 1200-tall framebuffer. That is what a
mirror onto a taller source looks like. Whether it was ever presented to the
user as working is unknown.

**Next step:** retry now that the service answers, and capture what the route
reports alongside what Display1 returns from `Stage`/`Confirm`.

Separately: **why did the display service wedge?** A hung Display1 with a
healthy-looking unit is its own defect and nothing in its journal explains it.

---

## 8. Settings Display route binds against a null model — FIXED (`89bf139d`)

Filed as "cosmetic when transient at page load". It was neither. A thrown QML
binding does not merely log — the whole binding fails, so in
`DisplayMirrorRow` `candidates` evaluated empty and **the mirror control
disabled itself**, which matters given item #7.

A new test builds `DisplayPage` with `displaySettings: null` and asserts no
binding errors. It found **57 dereference sites across seven QML files**, not
the three this file named. Optional chaining alone was not enough and the test
proved it: a guarded chain yields `undefined`, which QML cannot assign to a
typed `bool` or string property, so 23 sites merely traded `TypeError` for
`Unable to assign [undefined] to bool`. Both are now asserted absent.

---

## 9. `qindaqt-settings` and `xdg-desktop-portal-kde` crashes — OPEN, and ongoing

Still not investigated, and **`xdg-desktop-portal-kde` is crashing now**, not
just historically — seven SIGABRTs today between 01:13 and 01:16:

```
2026-09-22 01:13:35  SIGABRT  /usr/libexec/xdg-desktop-portal-kde
2026-09-22 01:16:33  SIGABRT  /usr/libexec/xdg-desktop-portal-kde   (x6 within one second)
```

Six aborts inside one second is a restart loop, not six independent faults.
The portal is what backs file pickers and screen sharing, so this is likely to
be user-visible.

`/var/lib/systemd/coredump` is now **853 MB**, up from the 752 MB recorded
under Housekeeping.

---

## 10. `Hybrid interaction failed: container move has no active baseline` — FIXED (`(pending)`)

**128 occurrences** in the current session log, from
`hybridcontainerplacement.cpp:213` and `:272` (a third site at `:331` covers
resize).

**Mechanism: it is what dragging a *maximized* window sounds like.** The log
gives the sequence directly:

```
775: QindaQt Hybrid interaction failed: restore a maximized container before moving it
776: QindaQt Hybrid interaction failed: container move has no active baseline
777: QindaQt Hybrid interaction failed: container move has no active baseline
778: QindaQt Hybrid interaction failed: container move has no active baseline
```

`Begin` is refused for a maximized container, so nothing is inserted into
`m_moveDrags`. The interaction controller then sends `Update` for the rest of
the gesture, and every one of them fails the `m_moveDrags.find()` lookup and
logs. The 128 lines are the echo of a handful of refused drags, one per pointer
motion event.

Nothing is broken — the window correctly does not move, which is why it "appears
to continue working". It is pure log noise, and it drowns the same channel that
would carry a real interaction failure.

**The contract decision: a refused `Begin` suppresses the remainder of that
gesture.** `Begin` still reports exactly why, once. The controller then records
the container, and absorbs the following phases of that gesture rather than
repeating itself per motion event. The record is cleared at the next `Begin`,
`Commit` or `Cancel`.

Deliberately narrow: a missing baseline with **no refused `Begin` before it** is
still reported, because that is a real anomaly and swallowing it would trade
this noise for a blind spot. Both the move and the resize paths are covered, and
every refusal reason records — maximized, shaded, and a failed `beginDrag`.

Covered by `compositor.hybrid-container-placement`
(`absorbsTheRestOfAGestureWhoseBeginWasRefused`), which asserts the one-line
refusal, five silent motions, clearing at `Commit`, the orphan case still
reporting, and a successful `Begin` leaving no suppression behind.

---

## 11. Dead signal in the visibility animation producer — FIXED (`d2557ba7`)

`PanelVisibilityAnimationProducer::reconcileRequested` was emitted and
connected to nothing. Connected rather than deleted: `synchronize()`'s
`immediateReconcile` out-param covers the moment a fade *starts*, and a fade
*ends* later with no synchronous caller to return to, so the signal was the
right mechanism and was simply unwired.

The reconcile did happen, but only because the fade-completion callback
releases the visibility-hold lease and that release happens to emit
`PanelInteractionStore::interactionsChanged`. Nothing declared that, so a later
change to the hold's lifetime would have left faded-out panels mapped at zero
opacity — invisible but still taking input.

---

## 12. Visibility reconcile cost is O(entire panel QML tree) — FIXED (`ea606d3d`)

`PanelVisibilityPointerProducer`'s filter is installed on the
`QGuiApplication`, so Qt runs it for every event of every object in the
process. It looked the object up *before* checking the event type, spending two
dynamic property lookups and an `objectName()` comparison on events that could
never match. The type check now comes first.

`synchronizePopupObjects()` walked `findChildren<QObject *>()` over every panel
window on every synchronize. It is now gated on a dirty flag set by
`QEvent::ChildAdded` — a popup can only enter a tree by being parented into it,
so the trigger is sound rather than a heuristic.

---

## 13. One blur manager global is bound per panel window — FIXED (`d2557ba7`)

`org_kde_kwin_blur_manager` is a Wayland global and `PanelSurfaceBlur` is
constructed per panel window, so the shell bound one copy per panel — and
republishes the whole panel set on every output-generation change. One binding
now serves the process, held by `weak_ptr` so it dies with the last panel
rather than outliving `QGuiApplication`.

---

## 14. Pre-existing test failures — HALF FIXED (`06e2370c`)

**`desktop.virtual.stage-closure` — fixed.** Not baseline drift. The Clipboard
and Task List staging for the `DesktopVirtual` component sat inside a block
guarded on `QINDAQT_WESTON AND QINDAQT_WESTON_SCREENSHOOTER`. Neither module
has anything to do with Weston, so on a machine without it the component staged
incomplete while the closure test, which needs no Weston, ran anyway and
failed. Removing the guard surfaced two more modules that had never been staged
at all: `QindaQt.Shell.GatherOverview` and `QindaQt.Shell.ObsApplet`.

**`qindaqt.controls-visual-125-*` / `-150-*` — still open.** 14 rows, QtQuick
Controls baseline drift at fractional scale, e.g. *"baseline drift: 9 pixels,
max channel delta 15"*. Deliberately untouched: regenerating baselines would
make the rows green whether or not something really shifted under Qt 6.11.1,
and telling those apart needs a look at the diffs, not a refresh.

**Newly found and fixed, not previously listed: 28 rows that only ever passed
because a display happened to exist.** A full `ctest` run on a headless shell
gave 41 failures out of 1016. Only 14 of them were this item's baseline drift:

| rows | cause |
| --- | --- |
| 14 | `controls-visual-125/-150-*` baseline drift — genuine, still open |
| 28 | construct a `QGuiApplication` without naming a platform, so they default to `xcb` and abort before their first assertion |
| 2 | `obs-bridge-*` — no libobs in this environment |

The 28 were `compositor.pointer-corner`,
`compositor.chrome-appearance-palette`, `compositor.hybrid-chrome-plan-builder`,
and 25 more across `file_manager` (9), `clipboard_applet` (5),
`font_preferences` (5), `system_monitor/view` (3), `audio_applet`,
`smart_lights_applet` and `voice_applet`. Every one passes under `offscreen` —
verified before and after — so none of them was ever testing what its failure
suggested. All are now pinned, with an `AGENT-GUARD` in each file saying why a
new GUI row needs the same treatment.

This is why a "pre-existing failures" item is worth keeping honest: 28 of the
41 were environmental, and they were loud enough to make the 14 real ones look
like part of the same noise.

---

## 15. Branch not merged — OPEN

The r9 and r10 ebuilds pin commits on
`fix/panel-visibility-and-hotplug-defects`, not on `main`. Merge and re-pin, or
the overlay depends on a branch. The branch has since grown eight more commits.

---

## 16. The shell and the compositor disagree about the output set — OPEN, new

**You will see:** panels pinned visible even when item #0 is not in play.

`OutputInventoryMatcher` refuses to evaluate a mixed generation, which is
correct, but it is refusing constantly. Current session log:

```
   1574  safe-visible output fallback: output 'DP-1' scale differs
    205  safe-visible output fallback: output 'eDP-1' scale differs
    102  safe-visible output fallback: output inventory counts differ
     17  safe-visible output fallback: output ... logical geometry differs
```

Measured directly, as an ordinary Wayland client against the session
compositor, KWin advertises **one** output:

```
wl_output#28.mode(1, 1920, 1080, 60000)
wl_output#28.scale(1)
zxdg_output_v1#29.logical_size(1920, 1080)
zxdg_output_v1#29.name("eDP-1")
```

and `QScreen` agrees: `eDP-1`, 1920x1080, `devicePixelRatio 1`. The
compositor's own D-Bus `ShellVisibilitySnapshot` at generation 45 described
*two* outputs, both 1920x1200, with `eDP-1` at scale 0.9. Same process, two
descriptions.

**`wl_output.scale` is 1 even under fractional scaling** —
`wp_fractional_scale_manager_v1` is bound, so the fractional ratio never
reaches `wl_output.scale`, and Qt's `devicePixelRatio` is that integer buffer
scale.

This matters for ADR-0234. That ADR accepts a match when
`ceil(compositorScale) == qtScale`. For a compositor logical scale of 1.25 —
which is what both displays were configured at — `ceil` is 2 while Qt reports
1, so the match fails and the shell falls back. That is consistent with 1574
rejections naming `DP-1`, and it means the r10 fractional-scale fix does not
cover the case it was written for.

**Confidence:** the wire measurement and the `wl_output.scale(1)` behaviour are
directly observed. The 1.25 arithmetic is inference — the outputs are not
currently at 1.25, so the failing comparison was not caught in the act.

**Next step:** set an output to 1.25, read `QScreen::devicePixelRatio()` and
the compositor's reported scale, and decide whether the two quantities should
be compared at all rather than which rounding to use. If they should not, that
supersedes part of ADR-0234.

---

## Unverified from the original report

The original complaint was panel animation degrading to roughly 1 fps over a
session. The largest measured drain was found and fixed in r10 — an orphaned
`qindaqt-xembed-tray-proxy` spinning on a dead XCB descriptor at 100% of a
core. **It was never confirmed which animation was dropping frames.** Idle CPU
on the running shell now samples at 0% of a core, so the standing drain is
gone; items #2 and #5 remain the candidates for the animation itself.
