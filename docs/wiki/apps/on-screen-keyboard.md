# On-Screen Keyboard

`qindaqt-osk` is the keyboard a finger gets on a touch display. The compositor
launches it as its input method (ADR-0204); it is not started by the user,
appears when a text field is focused by a finger or a pen, and leaves when a
hardware key is typed or the field loses focus.

## How it appears

1. `qindaqt-session` seeds `kwinrc [Wayland] InputMethod=` with the keyboard's
   desktop entry the first time a session starts; an existing choice is kept.
2. KWin starts `qindaqt-osk` on a dedicated Wayland connection that offers
   `zwp_input_method_v1` and `zwp_input_panel_v1`. Launched anywhere else the
   keyboard exits with status 4 and a message.
3. When an application's text field is focused, KWin hands the keyboard a
   context; the keyboard maps its panel. KWin parks the panel along the
   bottom of the output, centred, and shows it only when the last input came
   from touch or a tablet tool — the same rule Plasma uses.
4. A hardware key press hides the panel (the compositor plugin's interaction
   filter calls `InputMethod::hide()`); the next text field focused by a
   finger shows it again. Settings → Input → Touch can switch the keyboard
   off, which stops it (ADR-0205); there is no "always" mode, because the
   compositor's panel gate is not reachable from a plugin.

## Keys

Letters commit text. **Shift** is a one-shot: the next letter is upper case,
then the keyboard returns to lower case, as on a phone. **?123** switches to
the symbols page and **ABC** back; shift on the symbols page reveals a second
set of symbols. **Return** and **Backspace** travel as keysyms, which KWin
converts into key events for the focused client. The layout key shows the
current layout's label and switches to the compositor's next layout; the
last key hides the keyboard until the next field is focused.

Layouts are documents under `src/apps/osk/layouts/` (us, gb, de, fr, es).
The keyboard follows `org.kde.KeyboardLayouts` on the session bus; a layout
without a document falls back to `us`.

## Appearance

Surfaces, outlines, radii, spacing and type come from the design tokens the
Appearance route publishes, through the same Settings1 subscription and
theme controller every first-party app uses. Keys are at least 48 px tall.

## Evidence

- `qindaqt.osk-keyboard-model` (unit): documents, placement, shift, pages,
  key emission.
- `session.sessiondefaults`: the kwinrc seed.
- `compositor.touch-osk.osk.gtk-entry.single-1080p` and `…-1440p-125`
  (nested, private compositor): KWin launches this build's keyboard from a
  desktop entry the row writes; a finger taps a GTK entry (`oskShowsOnTouchFocus`,
  `keyboardReportsActivated`), fingers on the keys type `q`, a shifted `Q`
  and a backspace (`oskKeyCommitsText`, `oskShiftTypesUpperCase`,
  `oskBackspaceDeletes`), an injected hardware key hides the keyboard and
  reaches the entry (`hardwareKeyHidesOsk`, `hardwareKeyReachesEntry`), and
  the second entry brings it back (`oskReturnsOnNextTouchedEntry`). Captures
  `01-entry-mapped` … `05-osk-returns` land under `sv/evidence/`.

For debugging, `--evidence <file>` (or `QINDAQT_OSK_EVIDENCE_FILE`) makes the
keyboard write its visibility, panel size and placed key rectangles as JSON
whenever they change; `--layout <name>` pins a layout.
