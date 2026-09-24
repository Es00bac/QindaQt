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
`qindaqt-desktop-controls` uses KIdleTime to start that saver and consults
`org.freedesktop.ScreenSaver.GetActive` so it does not launch over an existing
lock; KScreenLocker remains the automatic idle-lock authority.

## Decision

- Preview every discovered saver by starting its own catalog program with the
  catalog's exact argument list, regardless of whether the lock screen has a
  scene for that saver. Do not start a lock-screen or greeter process from
  Settings.
- Preview `blank` with a Settings-owned full-screen black window on each
  available screen. Every available screen must show successfully; if one
  fails, close all opened windows and report failure. Any key (including
  Escape), click, or pointer movement closes it. If keyboard activation is not
  confirmed within one second, close it and report failure. It does not start a
  child process.
- Keep `none` and unknown tokens unavailable. Only discovered catalog entries
  may supply a program to the process boundary.
- Hold KScreenLocker's standard `org.freedesktop.ScreenSaver.Inhibit` request
  for the preview's lifetime. This is the public interface KScreenLocker
  itself honors before its automatic idle-lock timeout. Acquire it before
  showing or launching a preview; if acquisition fails, refuse to start. Send
  `UnInhibit` on every completion path, and keep the request on a dedicated
  session-bus connection so Settings exit also drops the request. Manual lock
  actions remain available and the preview never asks the lock service to lock.
- Bound every preview to 60 seconds. This guarantees a blank preview closes
  when input or keyboard focus is unavailable and prevents a stuck saver from
  holding idle inhibition indefinitely. Preserve and report a saver crash or
  nonzero exit code on the Settings page.
- Keep previews single-instance. Disable Preview while a preference write is
  in flight and report launch failures in the Settings page.

## Consequences

- Preview shows the same unlocked saver rendering that the session launcher
  starts after idle; lock-screen wallpaper behavior remains a separate choice
  described by ADR-0216 and ADR-0226.
- A saver package should continue to handle input in its `--screensaver` mode
  so the process exits when the user returns; the 60-second deadline bounds a
  saver that does not. Blank input dismissal is owned by the Settings process.
- The preview boundary is tested with an injected program resolver and fake
  executables, including every discovered entry's token and exact catalog
  arguments, nonzero exit reporting, automatic end, denied activation, and
  partial multi-display failure cleanup. A fake ScreenSaver D-Bus service
  verifies inhibitor acquisition and release, including Settings teardown.
  The Settings route must not invoke the greeter.
- Update the Screen saver Settings page and the ADR-0226 status note whenever
  this preview contract changes.

## Revisit when

Reconsider if the session idle launcher changes its public catalog launch
contract or if Settings gains a separately named, explicit lock-screen preview
action.
