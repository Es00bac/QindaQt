# ADR-0317: Own the native global shortcut registry

## Status

Accepted for the PF22–PF24 migration.

## Context

QindaQt's KWin fork currently embeds KGlobalAccelD in
GlobalShortcutsManager; KWin, the shell, desktop-controls and KF6 clients use
the org.kde.kglobalaccel and org.kde.kglobalaccel.Component D-Bus interfaces.
Settings Input and the GlobalShortcuts portal also depend on that
compatibility service. Removing the daemon before replacing its registered
action and client behavior would make those shortcuts stop working.

## Decision

Build a small policy/store core inside the compositor fork before changing the
runtime wire. PF22's KWin::Shortcuts::Registry owns stable component/action
bindings, named conflict refusal, component lifecycle cleanup, repeat policy,
and dispatch admission for locked or shortcut-inhibited input contexts. Its
Store reads and writes versioned shortcut values with an atomic file replace
and records whether legacy import has completed. Existing native values take
precedence over legacy import values.

The PF22 core does not alter active KGlobalAccel registration or advertise a
partial replacement. PF23 connects the core to KWin's synchronous input path,
implements the complete compatibility surface used by the compositor and
current clients, and imports kglobalshortcutsrc through a concrete adapter.
KGlobalAccelD remains until that compatibility path is qualified. PF24 moves
Settings and portal consumers to org.qindaqt.Shortcuts1, then removes the
separate kglobalacceld supervisor child.

The core stores complete Qt key sequences in portable form. KWin's input
adapter is responsible for translating compositor key events to the same
layout-independent sequence values and honoring press/release, modifier-only,
repeat, lock, and keyboard-shortcuts-inhibitor semantics.

## Consequences

- A conflict is returned with the incumbent owner and never partially changes
  an existing binding.
- Removing a component releases all of its bindings before its owner lifetime
  ends.
- The user continues to use the existing KGlobalAccel endpoint until the
  complete PF23 compatibility path is ready.
- Third-party or foreign desktop shortcuts are not claimed beyond the
  explicitly implemented QindaQt registration boundary.

## Verification boundary

The PF22 focused test covers registration, dispatch admission, named conflict
reporting, atomic reassignment refusal, owner removal, persistence round-trip,
one-time import, native value precedence, and malformed-store preservation.
Private-bus compatibility calls, actual compositor key interception, complete
KF6 client compatibility, Settings1 and portal integration belong to later
milestones.

## References

- Project roadmap: plasma-free QindaQt, section 3.7.
- [Input Settings](../apps/input-settings.md)
- [Input and shortcut Settings, ADR-0134](0134-input-and-shortcut-settings.md)
