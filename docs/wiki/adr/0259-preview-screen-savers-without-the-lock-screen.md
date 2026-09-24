# ADR-0259: Preview screen savers without the lock screen

- **Status:** Accepted
- **Date:** 2026-09-23
- **Owners:** Settings Center, Desktop controls
- **Supersedes:** ADR-0226 clause 4, which showed lock-drawable savers through the testing greeter
- **Superseded by:** None

## Context

Settings → Screen saver offers an explicit Preview action. ADR-0226 clause 4
sent savers drawable by the locker's wallpaper plugin, plus `blank`, through
`kscreenlocker_greet --testing`. Although testing mode did not acquire the lock,
it displayed the password screen and made the preview appear locked. Savers
without a lock-screen scene already ran as their own program.

The session's unlocked idle path launches every discovered saver from its
`ScreensaverCatalogEntry`: the entry's program token and the argument list
owned by the public desktop-controls catalog. Settings can use this same
contract without depending on the session launcher's private implementation.

## Decision

- Preview every discovered saver by starting its own catalog program with the
  catalog's exact argument list, regardless of whether the lock screen has a
  scene for that saver. Do not start a lock-screen or greeter process from
  Settings.
- Preview `blank` with a Settings-owned full-screen black window on each
  available screen. Any key (including Escape), click, or pointer movement
  closes it. It does not start a child process.
- Keep `none` and unknown tokens unavailable. Only discovered catalog entries
  may supply a program to the process boundary.
- A preview never asks the lock service to lock, changes the idle policy, or
  inhibits the real idle lock. The session's existing lock timeout continues
  to be owned by its normal authority.
- Keep previews single-instance. Disable Preview while a preference write is
  in flight and report launch failures in the Settings page.

## Consequences

- Preview shows the same unlocked saver rendering that the session launcher
  starts after idle; lock-screen wallpaper behavior remains a separate choice
  described by ADR-0216 and ADR-0226.
- A saver package must continue to handle input in its `--screensaver` mode so
  the process exits when the user returns. Blank input dismissal is owned by
  the Settings process.
- The preview boundary is tested with an injected program resolver and fake
  executable, including every discovered entry's token and exact catalog
  arguments. Blank preview tests cover its black full-screen window and input
  dismissal, and the Settings route must not invoke the greeter.
- Update the Screen saver Settings page and the ADR-0226 status note whenever
  this preview contract changes.

## Revisit when

Reconsider if the session idle launcher changes its public catalog launch
contract or if Settings gains a separately named, explicit lock-screen preview
action.
