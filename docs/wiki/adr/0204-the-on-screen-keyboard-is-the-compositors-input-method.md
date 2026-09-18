# ADR-0204: the on-screen keyboard is the compositor's input method

- **Status:** Accepted
- **Date:** 2026-09-17
- **Owners:** Platform (compositor input, first-party apps)
- **Supersedes:** None (completes the touch contract of [ADR-0193](0193-a-finger-is-the-left-button-and-a-held-finger-the-right.md))
- **Superseded by:** None

## Context

A touch display without a hardware keyboard could not enter text: QindaQt
shipped no on-screen keyboard and depended on nothing that provides one.
KWin already implements the compositor half of `input-method-unstable-v1`:
it launches one program named by `kwinrc [Wayland] InputMethod=` on a
dedicated Wayland connection, hands that program a text-input context while
an application's text field is focused, parks any `zwp_input_panel_v1`
surface along the bottom edge of the output, and shows that panel only when
the last input came from a finger or a pen. The choice was between adopting
Maliit (a new dependency with its own theming and its own animation
opinions) and writing a small first-party keyboard against the protocol KWin
already speaks.

## Decision

1. **`qindaqt-osk` is the input method.** The session seeds
   `[Wayland] InputMethod=` with the keyboard's desktop entry (the installed
   `org.qindaqt.OnScreenKeyboard.desktop`, or the file a private session
   names through `QINDAQT_OSK_DESKTOP_FILE`). It is a seed, never an
   override: a user who chose another virtual keyboard keeps it. No seed is
   written when no entry exists.
2. **The keyboard speaks the protocol itself.** It binds `zwp_input_method_v1`
   and `zwp_input_panel_v1` on the connection the compositor made for it and
   refuses to run anywhere else. Its window carries the input-panel role
   through Qt's Wayland shell-integration seam (`QWaylandWindow::setShellIntegration`),
   so Qt never requests an xdg toplevel for it. Text keys travel as
   `commit_string`; Return and Backspace travel as keysyms, which KWin turns
   into key events for text-input v3 clients.
3. **KWin decides when the panel shows; QindaQt decides when it hides.** The
   keyboard maps its panel whenever it holds a context and unmaps it when the
   context is taken away; KWin allows the panel only when the last input was
   touch or tablet. The compositor plugin hides the panel when a hardware
   keyboard key is pressed (keys the on-screen keyboard forwards never pass
   through the input filters, so they cannot hide it). Settings can only
   stop or restore the keyboard (`input.touch.onScreenKeyboard`, ADR-0205);
   KWin's panel gate is not exported, so an "always" mode is not offered.
4. **Layouts follow the compositor.** The keyboard reads the current layout
   and the layout list from `org.kde.KeyboardLayouts`, shows the matching
   document from its compiled set, falls back to `us` for a layout it has no
   document for, and switches layouts through the same interface. Its
   surfaces and type follow the design tokens like every first-party app.

## Consequences

- One new executable (`src/apps/osk`) and no new runtime dependency beyond
  Qt's Wayland client module, which the shell already requires.
- The keyboard depends on a private Qt API for the surface role; the build
  pins the Qt version, and the app is skipped with a notice where the private
  module is absent.
- A panel hidden by a hardware key returns on the next text-input enable,
  which every toolkit sends when a field is focused by a finger.
- The nested rows `compositor.touch-osk.*` prove the whole path inside the
  private compositor: KWin launches this build's keyboard, a finger on a GTK
  entry shows it, fingers on its keys type, a hardware key hides it, the next
  touched entry brings it back.

## Verification

- `qindaqt.osk-keyboard-model`: layout documents, placement, shift, pages and
  key emission without a compositor.
- `session.sessiondefaults`: the `[Wayland] InputMethod=` seed, its absence
  without an entry, and an existing choice left alone.
- `compositor.touch-osk.osk.gtk-entry.*`: the nested rows above, with
  framebuffer captures of the shown and hidden keyboard.

## Revisit when

- KWin gains `input-method-v2` or drops the v1 protocol: the keyboard's
  protocol layer is one file and would follow.
- Word prediction or an emoji page is wanted: the layout document format
  grows before the keyboard does.
