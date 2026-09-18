# ADR-0205: touch edges and touch preferences belong to the compositor

- **Status:** Accepted
- **Date:** 2026-09-17
- **Owners:** Platform (compositor input, Settings, shell runtime)
- **Supersedes:** None (completes [ADR-0193](0193-a-finger-is-the-left-button-and-a-held-finger-the-right.md) and [ADR-0204](0204-the-on-screen-keyboard-is-the-compositors-input-method.md))
- **Superseded by:** None

## Context

A finger swiped in from a screen edge is the touch equivalent of a hot
corner and of the panel's most-used buttons: on a tablet there is no pointer
to reach a corner with. KWin owns edge detection (its `ScreenEdges` reserves
touch edges for `QAction`s and recognises the swipe), but the surfaces a
QindaQt swipe should open — the overview popup, the notification center —
live in the shell process, and the task switcher is KWin's own, reachable
only through a global shortcut. The thresholds of ADR-0193 (how long a
finger is held for the menu) and the on-screen keyboard's showing rule of
ADR-0204 also needed a home a user can edit.

## Decision

1. **The compositor plugin reserves the touch edges.** One `QAction` per
   edge is reserved on KWin's `ScreenEdges` while the edge has an action; a
   completed swipe is announced as
   `org.qindaqt.Compositor1.EdgeGestureTriggered(edge, action)`. The plugin
   never opens anything itself. Defaults: left → overview, top →
   notifications, bottom → task switcher, right → nothing. KWin's own
   `[TouchEdges]` and `[TabBox] TouchBorderActivate` are left alone so no
   edge has two owners.
2. **The shell dispatches.** The shell runtime subscribes to the signal and
   maps `overview` to the overview applet's popup, `notifications` to the
   notification center toggle, and `task-switcher` to KGlobalAccel's
   `Walk Through Windows`. An action with no handler is reported, not
   guessed.
3. **`input.touch.*` is the single store.** Settings1 keys
   `input.touch.enabled`, `longPressMs`, `mode` (`auto`, `on`, `off`),
   `onScreenKeyboard` (`auto`, `off`) and
   `edgeLeft/Top/Right/Bottom` (schema v2). The compositor reads them through
   its own purpose-scoped client (ADR-0126/0129) and applies `enabled` to
   every touch device at KWin's seat (`InputDevice::setEnabled`: off is a
   real stop, fingers reach neither chrome, clients, edges nor the keyboard;
   only devices the plugin switched off are switched back on, so a device
   the user disabled in KWin's own device settings keeps that choice), the
   long-press threshold to the touch policy, the edge actions to the
   reservations, and the on-screen keyboard mode to KWin's input method
   (`off` stops it, `auto` keeps KWin's own rule: a finger or pen focused
   the field). There is no "always" mode: KWin's panel gate
   (`InputPanelV1Window::allow`) is not exported to plugins, so offering one
   would be a lie.
   The Settings → Input → Touch destination edits the same keys (all but
   `mode`) through the same kind of client, one write per edit, rows
   following the confirmed snapshot.
4. **`input.touch.mode` is stored ahead of its consumers, and offered by no
   control.** First-party applications keep the automatic rule of ADR-0193
   (touch-sized controls while a finger is in use) until their token facade
   can subscribe to the key without widening every application's appearance
   scope; the Touch page neither scopes nor shows the key until then, by the
   same rule that refuses an "always" keyboard mode.

## Consequences

- Edge gestures work in any session where the plugin and the shell run;
  the nested rows prove the compositor half without a shell, and the shell
  half is a pure dispatch table with its own row.
- Changing an edge action takes effect immediately in the running
  compositor; no reconfigure or re-login.
- `off` for the on-screen keyboard is a real stop of the keyboard process;
  switching back starts it again, so the preference is never merely cosmetic.
- Switching the touchscreen off is a real stop of every touch device at the
  seat (KWin persists a libinput device's enabled state the way its own
  device settings do); switching it back on restores exactly the devices
  this plugin stopped.
- The Touch destination's entry in the Input route's destination list is
  owned by the pen-and-tablet lane and is added when that lane lands.

## Verification

- `compositor.touch-edge-actions`: mapping, settings decoding, reservation
  bookkeeping, and the announced (edge, action) pairs.
- `compositor.touch-chrome.enabled.gtk-csd.single-1080p`: nested, the real
  resident Settings1 on the private bus; `input.touch.enabled=false`
  committed as a user transaction stops the seat's touch device (Compositor1
  reports the development input device unavailable and refuses a finger),
  `true` restores it. The row fails when the preference reaches nothing.
- `qindaqt.shell-edge-gesture-subscriber`: the dispatch table and the
  no-bus case.
- `compositor.touch-edges.edges.*`: nested, a finger swiped in from each
  edge of the private output; left, top and bottom are announced with their
  default actions, the right edge and an interior swipe stay silent.
- `qindaqt.settings-input-touch-model` and `…-touch-section`: the route's
  model and rows over a fake Settings1 transport.

## Revisit when

- KWin exposes the task switcher on D-Bus: the KGlobalAccel detour goes.
- Applications gain a scoped touch subscription: decision 4 is retired.
