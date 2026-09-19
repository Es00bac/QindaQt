# ADR-0215: the idle screensaver is decoration, and never a lock

- **Status:** Accepted
- **Date:** 2026-09-18
- **Owners:** Desktop controls, Settings Power route
- **Supersedes:** None
- **Superseded by:** [ADR-0216](0216-the-locker-draws-the-screensaver.md),
  for the lock-screen behaviour only: the locker now draws the saver itself.
  Everything else here stands.

## Context

QindaQt ships screensaver programs of its own — `x11-misc/qinda-patrol` and
`x11-misc/circuit-reef` when this was written, five of them since
2026-09-19. All are ordinary Wayland clients that place one surface per output
and exit on their own input; none authenticates anything. The session already has a lock authority — KWin's embedded
KScreenLocker, configured through the Screen lock section
([ADR-0091](0091-configure-kscreenlocker-preferences-through-settings.md),
[ADR-0132](0132-finish-session-locking.md)) — and a display-off policy that
belongs to PowerDevil
([ADR-0105](0105-delegate-idle-display-off-to-powerdevil.md)).

Every desktop that has merged these three concepts into one "screensaver"
preference has had to answer the same question afterwards: does the pretty
thing on the screen also protect the session? Here it must not. A saver is a
normal, unprivileged process; if it were allowed to sit between the user and
the password prompt, a crash, a stuck frame, or a program that simply refuses
to quit would be indistinguishable from a locked screen.

## Decision

The idle screensaver is decoration, owned by the resident
[`qindaqt-desktop-controls`](../architecture/desktop-controls.md) process, and
it is separate from both the lock and the display-off policies:

1. **Three preferences, three owners.** The saver pair
   (`power.screensaver`, `power.screensaverMinutes`) is its own
   purpose-scoped Settings1 client, distinct from `power.idleDisplayOffMinutes`
   and from `kscreenlockerrc`. Settings1 rejects a whole snapshot on one
   unknown key ([ADR-0126](0126-ignore-user-overrides-the-schema-cannot-normalize.md)),
   so widening an existing client would have put display-off behind this
   feature's schema risk.
2. **The locker keeps sole lock authority.** The launcher subscribes to
   `org.freedesktop.ScreenSaver.ActiveChanged` and stops the saver when the
   session locks. It never starts one while locked, so a saver is never the
   surface a returning user has to dismiss to reach authentication.
3. **A closed set of saver tokens.** `power.screensaver` holds a token, not a
   command line: `none` or one of the installed savers. An unrecognized
   persisted token — a stale value, a hand-edited file, a downgrade — reads
   back as `none`. Nothing from persistence ever reaches `QProcess` as a
   program name, and each known token carries its own fixed arguments
   (every output, telemetry off) rather than a user-editable argument list.
4. **Silence beats guessing.** Unlike display-off, which keeps its documented
   default while the Settings1 owner is absent, an unconfirmed screensaver
   preference starts nothing. Running a program the user never chose is worse
   than showing a plain idle screen.
5. **A saver that will not run is dropped, not respawned.** The launcher
   relaunches a saver that exits while the session is still idle (an output
   topology change ends one), but three exits inside five seconds stop the
   cycle until the next resume, and a `FailedToStart` (the package is not
   installed) stops it immediately.

## Consequences

- Turning a screensaver on has no effect on when — or whether — the session
  locks or the display sleeps. The Power route says so in the section itself,
  and a test asserts that sentence stays there.
- The saver packages are optional. With neither installed, the route still
  offers both names and the launcher reports the failed start once; the
  session is otherwise unaffected. Settings does not probe for installed
  saver binaries, so an unavailable choice is discovered by choosing it.
- Adding a saver means a new token in the schema's `allowedValues`, in
  `ScreensaverPreferences::knownSavers()`, in its `arguments()`, and in the
  route's option list — and, if it is to appear on a locked screen, a QML
  module and a scene file in the wallpaper plugin
  ([ADR-0216](0216-the-locker-draws-the-screensaver.md)). That is deliberately
  a handful of explicit edits rather than a discovery mechanism: the argument
  contract above is per-saver, and so is whether the greeter can draw it. The
  2026-09-19 set of five was added exactly this way.
- The launcher's own process lifetime (idle arming through KIdleTime, the
  quick-exit guard, the kill timer) has no focused test; its policy inputs
  and its preference seam do. Proving the idle path needs a session with a
  real idle-time backend.

## Revisit when

A saver needs to survive a lock (it must not), a user asks for an arbitrary
program as their saver (the token set exists to prevent exactly that, so this
would supersede the ADR rather than extend it), or the KIdleTime arming path
becomes testable and the launcher's guards deserve their own row.
