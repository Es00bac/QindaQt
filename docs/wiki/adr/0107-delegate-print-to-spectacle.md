# ADR-0107: Delegate Print to Spectacle

- **Status:** Accepted
- **Date:** 2026-09-07
- **Owners:** Session and platform
- **Supersedes:** the Print decision in [ADR-0100](0100-own-desktop-essentials-in-a-session-process.md)
- **Superseded by:** None

## Context

ADR-0100 put a detached Spectacle launch behind QindaQt's desktop-controls
`Print` action. The installed Spectacle desktop entry already registers Print,
owns its capture UI, and declares KWin's restricted screenshot interface.
Two registrations leave KGlobalAccel to choose one binding and can discard a
user's Spectacle remapping or capture-mode preference.

## Decision

The resident desktop-controls process does not register or launch a Print
shortcut. Installed Spectacle remains the sole provider of Print and its
normal capture workflow. The `ScreenshotLauncher` library fixture remains
available to its focused tests; it is not a production session dependency.

The private installed-session row prepares the normal private XDG applications
menu and KService cache needed for KWin to resolve Spectacle's desktop metadata.
Its disposable Spectacle profile selects a region capture and private automatic
save destination solely to decode the result; it does not modify a user's
profile, disable screenshot permission checks, or change production authorization.

## Consequences

- Print behavior follows Spectacle's user settings and remappings.
- QindaQt's volume/mute shortcuts remain independently owned by desktop
  controls; PowerDevil remains the brightness provider.
- Package and session composition must include Spectacle for Print support.
