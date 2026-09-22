# Open defects

Findings from the live multi-output session of 2026-09-21/22 that are **not
fixed**. Ordered by how likely you are to walk into them.

Four defects from that session *were* fixed and are live in
`gui-wm/qindaqt-desktop-0.1.0_pre20260921-r10` (branch
`fix/panel-visibility-and-hotplug-defects`): panels could never hide on a
fractionally scaled output; an orphaned XEmbed tray proxy burned a full CPU
core; a display hotplug deadlocked Meta+right-click customization; and a panel
pinned to one display erased every other panel while that display was absent.
Everything below is what remains.

Evidence throughout is from `~/.local/share/sddm/wayland-session.log`, which is
where the shell's stderr goes.

---

## 1. Meta+right-click dies whenever a pinned display is absent

**You will see:** in-place customization stops working — no panel menu, no
applet menu, no desktop menu — after unplugging or reconfiguring the display a
panel is pinned to. It does not come back until a layout-profile adoption
rebuilds the editor host.

This is the same root cause as the r10 fix, in a path that fix did not cover.
`ShellRuntimeApplication::reconcileSurfaces()` now hands the solver and the
visibility assembler a profile filtered through
`PanelProfileOutputOccupancy::presentOutputsOnly()`, but
`LiveCustomizationController::rebuildHost()` still passes the **unfiltered**
adopted profile:

```
src/shell/runtime/livecustomizationcontroller.cpp:147   m_host->rebuild(m_adopted, outputs);
src/shell/runtime/livecustomizationcontroller.cpp:149   m_host = std::make_unique<LiveEditorHost>(m_adopted, outputs, ...);
```

`LiveEditorHost` then fails to become ready, `available()` returns false, and
every chord entry point is gated on `available()` — so the deadlock described in
the r10 commit reappears through a different door.

Observed:

```
QindaQt shell live customization is unavailable: panel 'dock' names missing output 'eDP-1'
```

**Raised in priority by the r10 work**: the dock is now pinned to `eDP-1`, so
this fires whenever the built-in panel is off or being reconfigured.

**Fix:** filter through `PanelProfileOutputOccupancy::presentOutputsOnly()`
before `rebuild()`/`LiveEditorHost` construction, exactly as `reconcileSurfaces()`
does. Add a controller row that pins it.

---

## 2. The panel hide/reveal fade does nothing

**You will see:** panels and the dock *pop* in and out instead of fading. The
configured motion duration has no visual effect.

`QtPanelVisibilityAnimation::animate()` animates the `opacity` property of a
`QWindow`:

```
src/shell/runtime/panelvisibilityanimation.cpp:86
    auto *const animation = new QPropertyAnimation(&window, "opacity", this);
```

Qt's Wayland platform does not implement window opacity, so every transition
logs and the fade is a silent no-op:

```
This plugin does not support setting window opacity
```

224 of those in one 400-line stretch. This almost certainly never worked on this
platform; it only became visible once panels actually started transitioning.

Note the transition is still *correct* — mapping is what hides a panel, and the
`QPropertyAnimation` still runs and still fires `finished`, so the visibility
hold is released properly. Only the visual fade and the log noise are wrong.

**Fix options:** animate something the compositor honours (a layer-surface
margin slide, or an opacity animation on the QML root item rather than the
window), or drop the fade and the duration setting honestly. Do not leave a
setting that claims to do something it cannot.

---

## 3. The dock's input mask is a no-op, so transparent margins may eat clicks

**You will see:** clicks near the bottom of the screen swallowed by the dock's
invisible margins, across the full panel width, not just on the painted shelf.

`applyInputBounds()` narrows the panel window's input region to the painted
shelf plus hover allowance:

```
src/shell/runtime/runtimepanelwindowfactory.cpp:54    window->setMask(QRegion(bounds));
```

Qt's Wayland platform does not implement it:

```
This plugin does not support setting window masks
```

There is an `AGENT-GUARD` comment immediately above that call stating the mask
is what keeps a centered dock's transparent margins from blocking desktop input.
That guarantee does not hold on this platform.

**Fix:** set the Wayland input region directly (`wl_surface.set_input_region`,
the same way `panel_surface_blur.cpp` reaches past Qt for
`org_kde_kwin_blur`), or make the panel window only as wide as its painted
shelf so there are no transparent margins to mask.

---

## 4. One transient window kills the whole visibility snapshot

**You will see:** auto-hide stopping and starting for no obvious reason. Panels
stay visible for a while, then behave again.

`ShellVisibilitySnapshot` rejects the **entire** batch when any single managed
window fails validation:

```
src/compositor/src/shellvisibilitysnapshot.cpp:168
    fail(error, QStringLiteral("a managed window is invalid or ambiguous"));
```

The rejection is all-or-nothing, so a window that is momentarily off its
assigned output, or active while still marked minimized, discards the state of
every other window. The shell then selects safe-visible — every panel pinned
visible — until a clean snapshot arrives.

Observed 37 times in one pre-restart stretch, and 68 times earlier in the same
session:

```
QindaQt shell visibility snapshot is unavailable: a managed window is invalid or ambiguous
```

The all-or-nothing contract is deliberate (documented in
`docs/wiki/shell/panel-visibility.md`: "One bad member rejects the complete
batch"), so changing it is an architecture decision, not a patch.

**Fix:** decide whether a transiently invalid window should exclude *that
window* from the snapshot rather than void the batch, and record the decision as
an ADR either way. If the batch stays atomic, at minimum identify which window
and why, because the current message names nothing.

---

## 5. The shell leaks per display-hotplug

**You will see:** the shell getting slower and heavier over a long session,
especially if you plug and unplug displays. Animations degrade.

Measured across roughly one hour on the pre-fix shell, while `outputGeneration`
climbed from 9 to 15:

| | start | +28 min | +1 h |
| --- | --- | --- | --- |
| threads | 78 | 91 | 114 |
| RSS | 289 MB | 420 MB | 490 MB |
| Mesa GL contexts (`:gl0`/`:gdrv0`/`:traceq0` triples) | 20 | 24 | — |

Idle CPU rose from ~4.6% to ~23% of a core over the same window. Thread growth
comes in threes, which is one Mesa `util_queue` set per GL context, so roughly
three contexts leak per output-generation change — consistent with every panel
`QQuickWindow` being destroyed and recreated when the surface set is
republished.

`0c094c0d` pruned the stale `QPointer` list in the panel window factory, which
was a real but small part of it. **The context/thread growth itself is not
fixed.** It is not yet established whether the leak is in QindaQt (a window or
scene-graph resource outliving republication) or in Qt/Mesa (contexts not
released on `QQuickWindow` destruction).

**Next step:** republish the panel set N times in a controlled nested session
and count `/proc/<pid>/task` — a tight reproducer decides Qt-vs-QindaQt quickly.

---

## 6. `WindowsChanged` fires ~20 Hz on an idle desktop

**You will see:** background CPU use in the shell and compositor with nothing
happening.

Measured with `busctl --user monitor --match "path='/org/qindaqt/Compositor'"`:
**602 `WindowsChanged` signals in 30 seconds** on a desktop with no user input.

```
src/compositor/kwin/kwincontrolendpoint.cpp:112
```

The shell's visibility client coalesces invalidations, so it is not doing 20
snapshot fetches a second — but every consumer of that signal pays, and the
signal rate itself says something is re-notifying far more often than the
window state actually changes.

**Fix:** find what re-emits at that rate in `KWinHybridSession::handleWindowsChanged()`
and coalesce at the source.

---

## 7. Display mirroring fails

**You will see:** configuring mirrored outputs in Settings → Display not taking
effect, and the output generation churning as attempts roll back
(`outputGeneration` went 15 → 38 during the attempts).

**Not diagnosed.** What is known:

- Mirroring is implemented, not missing — draft (`setOutputMirror`,
  `display_settings_draft.cpp:210`), topology validation (`UnknownMirrorSource`,
  `MirrorSelfReference`, `MirrorCycle`), and a Display1 stage/preview/confirm
  transaction.
- The draft-level mirror logic does no mode or scale checking, so any rejection
  is at the Display1 service or the compositor.
- **The `Display1` service was hung** for the whole period of the attempts:
  D-Bus introspect timed out against `org.qindaqt.Display1` while the process
  was alive and its unit `active`. It was restarted at 23:14 and now answers.
  Every mirror attempt before that was doomed regardless of the configuration.
- The `DisplayMirrorRow.qml` null-binding `TypeError`s in the log are from hours
  earlier, not the recent attempts, so they are not the cause.
- Outputs: `eDP-1` 1920x1080@60 (Lenovo, scale 1.25), `DP-1` 1920x1200@59.885
  ("Dopesplay", scale 1.25), `HDMI-A-1` 1920x1080@60 (Wacom One Pen Display 13).

**Next step:** retry now that the service answers. If it still fails, capture
what the route reports and what Display1 returns from `Stage`/`Confirm`.

Separately: **why did the display service wedge?** A hung Display1 with a
healthy-looking unit is its own defect, and nothing in its journal explains it.

---

## 8. Settings Display route binds against a null model

**You will see:** display controls briefly inert or blank when the page opens.

Repeated `TypeError: Cannot read property '<x>' of null` from the Display and
Appearance routes, e.g.:

```
DisplayMirrorRow.qml:32: TypeError: Cannot read property 'outputs' of null
DisplayScaleSection.qml: TypeError: Cannot read property 'selectedOutput' of null
SettingsRouteHost.qml:53: TypeError: Cannot read property 'activeRouteAvailable' of null  (110 occurrences)
```

`DisplayMirrorRow.qml` dereferences `root.displaySettings.selectedOutput.<field>`
before the `?? ""` fallback can help, so a null `selectedOutput` throws and the
whole binding fails — `candidates` evaluates empty and the mirror control
disables itself.

Cosmetic when transient at page load; it is not established whether it is ever
persistent. `d5a6f9be` was an earlier pass at these null bindings.

---

## 9. `qindaqt-settings` and `xdg-desktop-portal-kde` crashes

Not investigated. Cores present:

```
2026-09-21 22:09:54  SIGABRT  /usr/libexec/xdg-desktop-portal-kde   (x3)
2026-09-21 22:37:33  SIGABRT  /usr/bin/qindaqt-settings             (22.5 MB)
```

---

## 10. `Hybrid interaction failed: container move has no active baseline`

Logged in long unbroken runs while dragging windows. Not investigated; the
interaction appears to continue working.

---

## 11. Dead signal in the visibility animation producer

`PanelVisibilityAnimationProducer::reconcileRequested` is emitted but has no
connection anywhere in the tree:

```
src/shell/runtime/panelvisibilityanimation.h:64    void reconcileRequested();
src/shell/runtime/panelvisibilityanimation.cpp:227 Q_EMIT reconcileRequested();
```

Harmless today — the reconcile it intends actually happens because releasing the
visibility-hold lease emits `PanelInteractionStore::interactionsChanged`, which
schedules the debounced reconcile. But it reads as the mechanism and is not, so
anyone reasoning about the hide path will be misled.

**Fix:** delete it, or connect it and drop the accidental reliance on the lease
release.

---

## 12. Visibility reconcile cost is O(entire panel QML tree)

Not a live symptom yet, but it scales the wrong way. Every
`PanelVisibilityRuntime::synchronize()` calls
`PanelVisibilityPopupProducer::synchronizePopupObjects()`, which walks the full
object tree of every panel window:

```
src/shell/runtime/panelvisibilitypopup.cpp:96    const auto objects = window->findChildren<QObject *>();
```

`settlePanelVisibility()` runs up to three `synchronize()` passes, and it runs on
every lease acquire/release that crosses zero. The panel tree grows over a
session (notifications, clipboard history, task list), so the per-transition cost
grows with uptime.

Also on the hot path: `PanelVisibilityPointerProducer` installs an event filter
on the `QGuiApplication` object, which Qt invokes for **every event of every
object in the process**, and it does two dynamic-property lookups plus an
`objectName()` comparison on each one
(`src/shell/runtime/panelvisibilitypointer.cpp:38-58`).

---

## 13. One blur manager global is bound per panel window

`PanelSurfaceBlur`'s constructor creates its own `BlurManagerExtension`:

```
src/panel_blur/src/panel_surface_blur.cpp:61    , m_manager(std::make_unique<BlurManagerExtension>())
```

`PanelSurfaceBlur` is constructed per panel window
(`runtimepanelwindowfactory.cpp:352`), so each panel binds its own
`org_kde_kwin_blur_manager` global instead of sharing one. Minor, but it is one
registry binding per window per republication, on a code path that already
republishes on every output-generation change (see #5).

---

## 14. Pre-existing test failures

Red on a clean tree — verified by stashing the session's changes and re-running,
so they are not from this work.

- `qindaqt.controls-visual-125-*` and `-150-*` — 14 rows, QtQuick Controls
  baseline drift at fractional scale, e.g. *"baseline drift: 9 pixels, max
  channel delta 15"*. Either the baselines need regenerating for the current Qt
  (6.11.1) or something really did shift.
- `desktop.virtual.stage-closure` — *"staged QML module
  QindaQt.Shell.ClipboardApplet has no qmldir"*.

---

## 15. Branch not merged

Both the r9 and r10 ebuilds pin commits on
`fix/panel-visibility-and-hotplug-defects`, not on `main`. Merge and re-pin, or
the overlay depends on a branch.

---

## Unverified from the original report

The original complaint was panel animation degrading to roughly 1 fps over a
session. The largest measured drain was found and fixed — an orphaned
`qindaqt-xembed-tray-proxy` spinning on a dead XCB descriptor at 100% of one
core, 4.6 CPU-hours accumulated — and shell idle CPU roughly halved (~23% → ~11%
of a core). **It was never confirmed which animation was dropping frames**, so
whether that specific symptom is gone is still open. Items #2, #5 and #12 are the
remaining candidates.

## Housekeeping

`/var/lib/systemd/coredump` held 752 MB, including two `qindaqt-shell` SIGSEGVs
caused during this session by pinning the dock to a display while the layout
solver still treated an absent pinned output as fatal (fixed in r10).
