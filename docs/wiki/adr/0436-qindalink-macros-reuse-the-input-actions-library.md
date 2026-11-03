# ADR-0436: QindaLink macros reuse the InputActions library

- **Status:** Accepted
- **Date:** 2026-11-02
- **Owners:** QindaLink (Platform workgroup)
- **Supersedes:** None
- **Superseded by:** None

## Context

QindaLink shows the phone a macro pad and lets the phone act as touchpad and
keyboard. [ADR-0433](0433-qindalink-phone-companion-architecture.md) already
decides that desktop input goes through the RemoteDesktop portal only, and the
architecture page first sketched a `link_macros` module with its own macro
editor. QindaQt already has one macro library: Settings → Input → Macros edits
`qindaqt/input-macros.json`, and controllers and keyboard triggers run its
macros ([ADR-0427](0427-share-programmable-input-actions-and-controller-layers.md)).
A second store would duplicate the editor, the validation limits and the
cancellation rules, and the user would maintain two libraries.

## Decision

1. **One library.** QindaLink keeps no macro store and has no macro editor.
   The link service reads macros only through the public
   `org.qindaqt.InputActions1` interface: `GetSnapshot()` for the
   version-1 snapshot and `Changed(revision)` to refresh. It never reads
   `qindaqt/input-macros.json` and never calls `Apply`, `Reset` or `Run`. The
   QindaLink app links to Settings → Input → Macros instead of editing.
2. **One runner.** The service runs a macro in-process with the shared
   `QindaQt::InputActions::MacroRunner`, over a `MacroOutput` implemented on the
   portal session. The runner's validation, hold pairing, bounded text batches
   and cancel-releases-everything behavior apply unchanged.
3. **Phone view.** The phone receives `id` and `name` of each macro and nothing
   else (`macro.list`). Steps, text and shortcuts stay on the desktop. A run is
   requested by id.
4. **One portal session.** Typing, pointing and macros share a single
   RemoteDesktop session through a reusable `InputSink` interface. That sink is
   the seam the vault feature uses later; the vault adds no second session and
   no second approval. The session is created only on a phone's explicit
   `input.begin`, uses `persist_mode` 2 with the restore token in a 0600 file
   under `$XDG_STATE_HOME/qindalink/`, and closes with its controller.
5. **Gate.** `MacroOutput::allowed()` is true only while the portal session is
   active and an `InputGate` is open. The production gate opens only on an
   authenticated native-compositor Unlocked receipt
   ([ADR-0320](0320-native-notification-lock-observation.md) pattern); an
   unknown or locked state is closed. A lock releases every held key and
   cancels the run immediately.
6. **Known limits.** A portal cannot see the focused window, so `target()` is a
   stable sentinel and the runner's "focus changed, cancel" rule does not fire
   for these runs. Compositor-only actions (maximize, close window, show
   desktop) cannot be performed through a portal and fail the run with
   `input-unavailable` instead of doing something else.

## Consequences

- Users edit macros in one place, and a macro saved in Settings appears on the
  phone without a QindaLink release.
- The link service depends on `InputActions1` being provided by the controller
  plug-in in the compositor; without it the phone sees an empty list.
- The first live use of the portal needs the user's approval prompt; no test
  uses the real portal. Tests register fakes of both services on a private bus.
- The `input` and `macro` namespaces are defined in the
  [protocol page](../reference/qindalink-protocol-v1.md) and described in the
  [architecture page](../architecture/qindalink.md).

## Revisit when

- The compositor exposes a window-action API that is safe for a user service,
  which would let portal-run macros keep the compositor-only steps.
- `InputActions1` gains a `Subscribe` or per-macro metadata (icon, colour) that
  the phone should show.
