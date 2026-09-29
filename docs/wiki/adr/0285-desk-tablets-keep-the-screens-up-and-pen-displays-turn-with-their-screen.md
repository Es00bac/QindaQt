# ADR-0285: Desk tablets keep the screen's up; pen displays turn with their screen

- **Status:** Accepted
- **Date:** 2026-09-28
- **Owners:** Tablet devices service; Session desktop controls; Settings Input route
- **Supersedes:** [ADR-0197](0197-pen-displays-map-themselves-and-ask-once.md), in part (the consequence that neither process needs a Display1 client)
- **Superseded by:** None

## Context

The owner asked for three things: rotate a tablet that is not a screen, and
know the difference; rotate a pen display with its screen; and map a chosen
rectangle of a desk tablet onto a chosen rectangle of the screen.

Facts that shaped the decision, verified against the sources and the
owner's hardware on 2026-09-28:

- **libinput 1.31.3 never offers its own rotation to a tablet.** Only the
  fallback dispatch (mice, trackballs, touchscreens) installs the rotation
  configuration (`evdev-fallback.c fallback_init_rotation`). Every tablet
  reports `supportsRotation = false`, so the old Rotation row, gated on that
  flag, was hidden for every tablet — the owner's complaint about the Wacom
  Bamboo Connect CTL-470.
- **KWin 6.6.6 can still turn a tablet** through its `orientation`, folded
  into the calibration matrix (`device.cpp setOrientedCalibrationMatrix`) and
  published over D-Bus as the int `orientationDBus`. It needs
  `supportsCalibrationMatrix`. Neither host has libwacom, and
  `dev-libs/libinput` is built without it, so libinput treats every tablet as
  a display tablet for calibration purposes: the Bamboo reports
  `supportsCalibrationMatrix = true`.
- **KWin maps an absolute tablet position through the mapped output's
  transform** (`connection.cpp devicePointToGlobalPosition`). That is right
  for a pen display, whose digitizer turns with its panel, and wrong for a
  desk tablet on a rotated monitor: its directions turn with the monitor.
  `mapToWorkspace` applies no transform; an `outputName` naming no present
  output falls back to the active output; `FlipX*` is treated exactly like
  the matching `Rotate*`.
- **The order is fixed:** left-handed flip, orientation (calibration), input
  area, output area (in the output's native mode frame), output transform.
  The input area therefore lives in the post-orientation frame and the output
  area in the unrotated panel's frame.
- **KWin stores `inputArea` without checking it,** and libinput refuses a
  rectangle with `x2 > 1.0` (computed as `x + width`) and keeps the old one,
  so kcminputrc and the device disagree after one rounding error.
- **libinput's own verdict on "is this a screen" is already published.**
  libinput offers a tablet area only when the evdev node is not
  `INPUT_PROP_DIRECT`, after its quirks corrected the kernel's properties
  (`evdev-tablet.c tablet_init_area`), and KWin publishes that as
  `supportsInputArea`. KWin 6.6 requires libinput 1.28, so the flag is
  meaningful on every supported system. (Bamboo: true; its touchscreen and
  any pen display: no area.)
- **`QScreen` cannot say how a screen is turned.** Qt's Wayland screen derives
  `orientation()` from the wl_output transform *and* whether the logical
  geometry is portrait, and ignores flips (qtbase 6.11
  `qwaylandscreen.cpp toScreenOrientation`). Display1 publishes the
  compositor's own transform per connector.

## Decision

**A desk tablet's up is the screen's up; a pen display turns with its
screen. One pure planner, shared by the session and Settings, turns what
the user asked for into KWin's values, and the session re-plans whenever a
screen turns or the mapping changes.**

- **Which kind.** A tablet tool is a desk tablet when KWin reports
  `supportsInputArea = true` and a pen display when it reports `false`
  (libinput's verdict). Only when the flag is absent does ADR-0197's output
  matcher decide (a matched own screen means pen display); otherwise it is a
  desk tablet. libwacom is not added: it is not installed on either host and
  would change libinput's behaviour (see Consequences).
- **Intent, not device values, is remembered.** For a desk tablet the
  existing `input.tabletMappings` record gains three optional members, all in
  the frames the user sees: `rotation` (how the tablet is turned clockwise on
  the desk, 0/90/180/270), `inputArea` (the used part of the tablet as it
  lies) and `outputArea` (the part of the mapped screen or workspace as it
  appears). A malformed member is dropped on its own. A missing rotation is
  upright; a missing area is adopted from the device through the same
  relation the planner writes it with, so adoption writes nothing.
- **The plan.** The compensation is the mapped output's rotation (a named,
  present output), the rotation every enabled output shares (following the
  active screen), or none (the whole workspace, or screens that disagree).
  The device's rotation is the user's turn composed with the inverse of the
  compensation — through libinput `rotation` when a future libinput offers
  it, otherwise KWin `orientationDBus`. The input area is the seen rectangle
  turned by (applied − user); the output area is the seen rectangle turned by
  the inverse of the compensation. Every area is normalized so `x + width`
  never exceeds 1.0 in double arithmetic. A rotation that is not known
  (Display1 has not answered) plans and records nothing.
- **Pen displays follow their screen.** Settings hides Rotation and
  Left-handed for them and says which screen they turn with, with a link to
  Displays. The session clears any rotation of their own (`orientationDBus`,
  libinput `rotation`, `leftHanded`), because KWin's output transform already
  turns them. They stay mapped by ADR-0197. Their screen area is shown as the
  turned screen shows it and written in the panel's own frame, never re-based,
  because the digitizer is fixed to the panel.
- **Where rotations come from.** Both processes decorate their `QScreen`
  output list with the transform Display1 publishes per connector
  (`DisplayRotationTabletOutputs`, a purpose-scoped Display1 client). Flips
  are dropped exactly as KWin drops them.
- **Who writes.** The session policy re-plans every tablet on start, hotplug,
  output change, ledger change and mapping change. Settings plans on every
  user edit (an unknown rotation is then treated as upright and the note
  says so) and on mapping changes (known rotations only). Both write KWin
  first and record the intent second, like mapping choices (ADR-0197).
- **The area editor** (desk tablets): the tablet drawn to its physical shape
  as it lies, the mapped surface drawn to its logical shape (each screen
  outlined for the workspace), a rectangle on each that moves when dragged,
  resizes from a corner, and is drawn anew on the empty surface; arrow keys
  move, Shift with the arrow keys resizes, Home fills. Keep proportions locks
  the screen rectangle to the tablet rectangle's physical shape. The smallest
  side is 5% of a surface.
- **Calibration** (the four-target wizard) is offered for pen displays only.

## Consequences

- The session process and the Settings Input route each gain a Display1
  client. Neither adds a process; if Display1 is down, rotations are unknown
  and nothing is re-planned until it answers. This supersedes ADR-0197's
  statement that neither process needs a Display1 client; output identity
  still comes from `QScreen`.
- **libwacom is a trap to spring knowingly.** Enabling libinput's wacom
  support (and installing `dev-libs/libwacom`) makes libinput withhold the
  calibration matrix from desk tablets; KWin's orientation then does
  nothing, the Rotation row disappears for them, and only Left-handed (a
  180° turn) remains. Classification stays correct either way. No Portage
  change is needed for this ADR.
- Screens rotated differently while a tablet follows the active screen get
  no compensation; Settings says so and suggests mapping to one screen.
- KWin keeps persisting the device values in kcminputrc; the ledger holds the
  intent. A value changed by another tool is re-planned from the intent on
  the session's next pass, the same "the ledger wins" rule as ADR-0197.
- Two identical tablets share one intent, the identity limit of ADR-0197.
- Tests: `qindaqt.services-tablet-devices-orientation`,
  `qindaqt.services-tablet-devices-placement` (an independent model of the
  KWin + libinput pipeline for all 16 turn × rotation pairs),
  `qindaqt.session-desktop-controls-tablet-orientation`,
  `qindaqt.settings-input-tablet-placement` and
  `qindaqt.settings-input-tablet-area-editor`, plus updated rows in the
  existing tablet suites.

## Revisit when

- libinput is built with libwacom, or libinput offers tablets its own
  rotation configuration.
- KWin compensates desk tablets itself, publishes a display-tablet flag, or
  changes how `devicePointToGlobalPosition` treats flipped transforms.
- The session can follow the active output per pen event, which would allow
  compensating screens that are rotated differently.
