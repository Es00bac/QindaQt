# ADR-0216: the locker draws the screensaver, so a locked session keeps showing it

- **Status:** Accepted
- **Date:** 2026-09-19
- **Owners:** Desktop controls, Settings Power route, Session
- **Supersedes:** the lock-screen half of
  [ADR-0215](0215-the-idle-screensaver-is-decoration-not-a-lock.md)
- **Superseded by:** None

## Context

[ADR-0215](0215-the-idle-screensaver-is-decoration-not-a-lock.md) kept the
screensaver strictly out of the locked session: the launcher stops the saver
when KScreenLocker activates, so a saver can never stand between the user and
the password prompt. That reasoning about *authority* was right and is kept
below. The behavior it produced was wrong: locking an idle machine replaces the
screensaver with the greeter's static wallpaper and a password field, which is
the opposite of what a screensaver is for. A screensaver and a lock screen are
the same moment to the person walking away from the desk.

Two ways exist to put the saver in front of a locked session:

1. **A process above the lock screen.** KWin 6.6.6 implements
   `kde_lockscreen_overlay_v1` (`LockscreenOverlayV1Interface::allowRequested`),
   so a client can ask for its surface to stay visible while locked.
2. **The locker draws it.** `kscreenlocker_greet` loads a KPackage wallpaper
   plugin named by `kscreenlockerrc` `[Greeter] WallpaperPlugin`, and the
   greeter's own UI — clock, password field, media controls — fades out after
   ten seconds of inactivity and returns on input
   (`fadeoutTimer` in plasma-workspace's `LockScreenUi.qml`).

## Decision

The locker draws the screensaver. QindaQt ships one wallpaper plugin,
`studio.qinda.screensaver` (`data/lockscreen/`, installed to
`share/plasma/wallpapers/`), whose QML loads the chosen saver's own
`QQuickItem`: `Qinda.Patrol`'s `PatrolScene` or `QindaQt.CircuitReef`'s
`CircuitReef`, each shipped as an installable QML module by its own package.

1. **No process is ever shown above the lock screen.** Option 1 was refused.
   The saver programs are ordinary unprivileged clients; a hung or crashed one
   holding a surface over the greeter is indistinguishable from a locked screen
   that will not take input, and `circuit-reef` is an SDL client that would
   need raw Wayland protocol code to ask for it at all. The launcher keeps
   stopping its process when the session locks — the greeter takes the picture
   over from there, so exactly one thing draws the saver at any moment.
2. **One choice drives both.** Settings writes the Settings1 pair *and*
   mirrors the token into the greeter's plugin configuration through
   `LockScreenSaverStore` (the Screen saver route since
   [ADR-0226](0226-configure-the-screen-saver.md); the Power route before it). Only a confirmed snapshot is mirrored, so a refused
   or uncertain commit never leaves the lock screen showing a saver the
   unlocked session does not have.
3. **The prompt is the greeter's, and it hides itself.** QindaQt adds no
   password UI and changes no locker timing. An idle locked screen shows the
   saver alone because the greeter fades its own UI out; touching the keyboard
   or mouse brings the prompt back over a blurred saver. This is why the
   feature needs no new lock-screen code and no new trust boundary.
4. **The scene degrades to a painted ground.** The plugin paints its background
   first and loads each saver through a `Loader` whose failure is logged and
   ignored. A missing or broken saver package leaves a plain lock screen, never
   a black one and never a greeter that fails to start.
5. **The greeter mirror is its own write set.** `[Greeter] WallpaperPlugin` and
   this plugin's `Saver`/`PreviousPlugin` keys only. `[Daemon]` — automatic
   locking, timeout, resume-lock, grace — stays with the screen-lock section's
   store (ADR-0091, ADR-0132). The plugin the greeter had before QindaQt took
   it over is recorded and handed back when the saver is turned off.

Everything else in ADR-0215 stands: the locker remains the only lock authority,
the saver token set stays closed, an unknown token never reaches `QProcess`,
and an unconfirmed preference starts nothing.

## Consequences

- A locked, idle machine shows the screensaver, and the password field appears
  when someone arrives. That is one feature built out of two existing
  behaviours; no QindaQt code runs on the lock screen.
- A saver package must install a QML module as well as its executable
  (`Qinda.Patrol`, `QindaQt.CircuitReef`) to appear on a locked screen. This
  is a real constraint, and the 2026-09-19 suite hit it immediately: the three
  SDL/OpenGL savers ship no `QQuickItem`, so `ScreensaverPreferences::
  showsOnLockScreen()` names the ones that do, choosing any other releases the
  greeter's wallpaper back to the user's own, and the Power section's status
  line says which of the two behaviours is in effect. A blank lock screen
  would have been the alternative, and is worse than an unchanged one.
- The greeter is a separate process that now loads QindaQt QML. A fault in a
  saver's scene is contained by the `Loader` above, but a crash inside the
  plugin's C++ would take the greeter down; KScreenLocker keeps the session
  locked when its greeter dies, so the failure mode is "no picture", not "no
  lock".
- Changing the saver takes effect at the next lock; the greeter reads its
  configuration when it starts. Nothing needs to be restarted.

## Revisit when

A saver is wanted that cannot provide a `QQuickItem`, the greeter gains a way
to host an external surface safely, or KScreenLocker changes how a wallpaper
plugin is selected.
