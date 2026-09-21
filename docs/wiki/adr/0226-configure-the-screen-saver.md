# ADR-0226: configure the screen saver

- **Status:** Accepted
- **Date:** 2026-09-21
- **Owners:** Desktop controls, Settings Center
- **Supersedes:** [ADR-0215](0215-the-idle-screensaver-is-decoration-not-a-lock.md),
  for the closed token set and the Settings placement only: the saver set is
  now discovered, and configuration moved out of the Power route. The
  decoration-not-a-lock authority split stands unchanged.
- **Superseded by:** None

## Context

The operator's words: *"The screensaver has no configuration really."* QindaQt
ships five screensavers as separate packages (`x11-misc/circuit-reef`,
`prism-brawl`, `prism-circuit`, `starward-reimagined`, `qinda-patrol`), and the
lock screen draws one of them through a KPackage wallpaper plugin
([ADR-0216](0216-the-locker-draws-the-screensaver.md)). What existed before
this change was a Screensaver section buried in the Power route whose saver
list was one hard-coded set in four places — the page's QML,
`ScreensaverPreferences::knownSavers()`, the launcher's argument table, and
the schema's `allowedValues` for `power.screensaver`. A newly emerged saver
could not be chosen until Settings grew the same edit everywhere, and there
was no way to see a saver without idling or locking the real session.

Two forces decide the shape:

- **Persistence can never name a program.** Settings1 stores a string token;
  the launcher turns it into a `QProcess`. The closed-set design proved that
  mapping, but paid for it with the four-place list above.
- **The screensaver is not the lock.** The saver delay, the lock decision, and
  the display-off policy are three preferences with three owners
  ([ADR-0215](0215-the-idle-screensaver-is-decoration-not-a-lock.md)). A
  configuration page must place the first two side by side without conflating
  them.

## Decision

Configuration lives on a dedicated **Screen saver** Settings route, appended
after Startup applications so no existing route index, shortcut, or traversal
position moves ([ADR-0128](0128-accessibility-settings-route.md)). The Power
route's Screensaver section is removed; its Screen lock section stays.

1. **The saver set is discovered, not enumerated.** Each shipped saver
   installs a `.desktop` entry. `DesktopEntryScreensaverCatalog` scans the
   system application directories (never the user-writable one) and admits an
   entry when it attests its purpose ("screensaver" in Keywords, GenericName,
   Comment, or an action name) and proves the launch contract (a desktop
   action that runs the entry's own program with `--screensaver` or
   `--all-screens`). The persisted token is the program name, which is what
   lets values written before discovery existed keep working. A newly
   packaged saver appears in Settings with no code change.
2. **Two built-in choices are reserved tokens, not packages.** `none` runs
   nothing; `blank` runs nothing while unlocked and shows the locker plugin's
   painted dark ground once locked. A program may never take either name.
3. **The schema stops listing savers.** `power.screensaver` keeps its string
   type and `none` default but drops `allowedValues`: an enumerated schema
   would reject every newly packaged saver at commit time, recreating the
   problem discovery solves. Boundedness moves to the catalog — an unknown or
   stale token reads back as `none` and never reaches `QProcess`
   (unchanged from ADR-0215). The resident settings service must be restarted
   once to pick up the relaxed schema, or writes of `blank` and newly
   discovered tokens are refused by the old in-memory one.
4. **Preview never locks the session.** Savers the locker's wallpaper plugin
   can draw (and `blank`) preview through `kscreenlocker_greet --testing`,
   the documented testing path; a saver with no lock-screen scene runs as its
   own program, exactly as it appears while idle and unlocked. Previews never
   stack, and the preview always shows persisted truth — a write in flight
   disables the button.
5. **Locking is its own section on the same page.** The walk-away section —
   whether the session locks at all, and after how long — uses the shared
   screen-lock model and store extracted from the Power route
   (`qindaqt_settings_screen_lock`), so both pages read and write the same
   `kscreenlockerrc` `[Daemon]` truth with merge-latest semantics. The
   section states plainly, with the reason, when that truth cannot be read or
   written. The greeter mirror writes only `[Greeter]` wallpaper keys, as
   before.
6. **The lock screen follows only savers it can draw.** The catalog's
   `showsOnLockScreen` table (a scene file in the wallpaper plugin is not
   discoverable from a desktop entry) decides whether a confirmed choice is
   mirrored into the greeter; any other saver, and `none`, hands the lock
   wallpaper back. The page's status line says which behaviour is in effect.

## Consequences

- The operator can choose a saver (including None and Blank screen), set how
  long the session idles before it starts, preview it safely, and set whether
  and when the session locks — all from one page.
- Packaging a new saver correctly (desktop entry with the attesting keyword
  and the launch-contract action) is sufficient for it to appear in Settings
  and run. Shipping a lock-screen scene additionally requires the wallpaper
  plugin scene and a `showsOnLockScreen` entry, as ADR-0216 already required.
- The schema edit is a persistence-contract relaxation. Old values (`none`,
  any of the five shipped tokens) remain valid; `blank` and newly discovered
  tokens are accepted only once the resident service reloads the schema. The
  catalogue seam, not the schema, is now the invalid-token fence.
- Focused rows cover catalog discovery rules, preference mapping, the model's
  commit/mirror/preview behaviour, the greeter store's write set, the preview
  process boundary, and the page's bindings; the navigation rows pin the new
  route's appended index.
- The Power route's page keeps its Screen lock section; nothing about
  automatic locking's *storage* changed, only which page may also edit it.

## Revisit when

A saver ships whose launch contract cannot be expressed through a desktop
action, the wallpaper plugin learns to enumerate its own scenes (making
`showsOnLockScreen` derivable), or KScreenLocker's testing greeter changes
its invocation.
