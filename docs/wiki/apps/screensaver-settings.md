# QindaQt Settings — Screen saver route

`qindaqt-settings --page screensaver` chooses what an idle screen shows and,
separately, whether and when the session locks. The design and the discovery
contract are [ADR-0226](../adr/0226-configure-the-screen-saver.md); the saver
itself remains decoration, never a lock
([ADR-0215](../adr/0215-the-idle-screensaver-is-decoration-not-a-lock.md)),
and the lock screen draws the chosen saver through the wallpaper plugin
([ADR-0216](../adr/0216-the-locker-draws-the-screensaver.md)). Preview follows
the separate [ADR-0259](../adr/0259-preview-screen-savers-without-the-lock-screen.md)
contract. The route was split out of the Power route, whose Screensaver
section it replaces.

## What the route shows

- **Screen saver** — a selector listing the two built-in choices, **None**
  and **Blank screen**, followed by every saver discovered from the installed
  packages' `.desktop` entries. Discovery is a rule, not a list: an entry is
  admitted when it attests its purpose ("screensaver" in Keywords,
  GenericName, Comment, or an action name) and proves the launch contract (a
  desktop action running the entry's own program with `--screensaver` or
  `--all-screens`). Only system application directories are scanned, so the
  persisted token can never resolve to a user-planted file. A **Start after**
  delay (1–240 minutes) is enabled only when a real program was chosen.
- **Preview** — runs each discovered saver as its own program with the same
  catalog arguments used by the unlocked idle path, regardless of whether the
  lock screen can draw that saver. Blank screen opens a full-screen black
  window on every available display. A partial show closes all opened windows
  and reports failure. Blank closes on any key (including Escape), click, or
  pointer movement; it also closes if keyboard focus is not confirmed within
  one second. Every preview ends after at most 60 seconds. A saver that crashes
  or exits nonzero reports its exit status and code on the page. The preview
  never starts the lock screen. It holds KScreenLocker's standard
  `org.freedesktop.ScreenSaver.Inhibit` request while running, so automatic
  idle locking cannot put the password screen over the preview; the request is
  released when the preview ends or Settings quits. If the lock service cannot
  grant the request, Preview does not start. The button is disabled while a
  preference write is in flight, a second preview cannot stack, and failures
  appear on the page.
- **Locking** — the walk-away section: whether the session locks
  automatically when idle, and after how long. This is the same shared
  screen-lock model and store the Power route's Screen lock section uses;
  the two pages can never disagree, because there is one `kscreenlockerrc`
  `[Daemon]` truth. When that truth cannot be read or written, the section
  says so with the reason.

The status line says which lock-screen behaviour is in effect: a saver with a
wallpaper-plugin scene keeps showing while locked, any other saver leaves the
lock screen's own wallpaper alone, and Blank screen is the plugin's painted
dark ground.

## Authority and write boundary

The saver pair (`power.screensaver`, `power.screensaverMinutes`) is read and
written through one purpose-scoped Settings1 client
([ADR-0126](../adr/0126-ignore-user-overrides-the-schema-cannot-normalize.md)
is why the pair gets its own client). `power.screensaver` holds a token —
`none`, `blank`, or a discovered program name — never a command line; an
unrecognized persisted token reads back as `none` rather than being offered
or launched. Since ADR-0226 the schema no longer enumerates savers in
`allowedValues`; the catalog is the invalid-token fence. The resident
settings service must be restarted once after that schema relaxation, or it
keeps refusing the new tokens with its old in-memory schema.

The settings page does not treat the desktop controller's safe runtime
fallback (`none`, five minutes) as a confirmed user choice. Before the first
valid Settings1 snapshot it shows no selected saver or delay and disables both
selectors, and hides Preview. A first snapshot equal to that fallback
still establishes authority. The page keeps the last confirmed choice visible, labeled as such,
when Settings1 becomes unavailable; editing remains disabled until the current
owner's snapshot and write lane are ready.

A saver or delay choice is admitted only while both scoped keys are writable.
The page shows the confirmed selection during a pending commit and waits for a
same-owner, same-epoch snapshot at or after the Applied revision before claiming
success. A valid older snapshot triggers a bounded refetch; if no qualifying
readback arrives within four seconds, the route ends its pending state as
uncertain without repeating the write. A refusal, conflict, lost reply, owner change, or readback mismatch
leaves the authoritative choice visible with a distinct diagnostic. An
unchanged or external refresh does not erase that diagnostic. **Try again**
only refreshes Settings1; it never repeats the write. Keyboard activation of
either selector returns to the confirmed value if the write is refused.

Only a confirmed snapshot is mirrored into the greeter's
`kscreenlockerrc` `[Greeter]` wallpaper configuration (the
`studio.qinda.screensaver` plugin id, its `Saver` key, and the recorded
`PreviousPlugin`), so a refused or uncertain write never changes what a
locked session shows; the `[Daemon]` group belongs to the walk-away section's
own store. The resident
[`qindaqt-desktop-controls`](../architecture/desktop-controls.md) process
starts and stops the chosen saver; the route starts no saver process itself
except the explicitly requested preview.

## What this route does not claim

- Preview never requests a lock or starts the greeter. Its bounded,
  preview-lifetime inhibitor prevents automatic idle locking from covering the
  preview; a failed inhibit request prevents preview startup. Manual locking
  remains a separate user action, and KScreenLocker remains the lock authority.
- The lock-screen take-over follows the saver only for savers that ship a
  QML scene in the wallpaper plugin; a saver without one leaves the lock
  wallpaper unchanged, and the page says so.
- A saver package that is removed after being chosen reads back as `none` at
  the next snapshot; no stale icon or ghost entry is shown.
- Lock-on-resume and the password-grace delay remain on the Power route's
  Screen lock section; this page's Locking section covers idle locking only.

## Verification

    ctest --test-dir .build --output-on-failure \
      -R '^qindaqt\.(settings-(screensaver|screen-lock)|session-desktop-controls-screensaver|settings-route-registry|settings-navigation)'

- `qindaqt.settings-screensaver-model`: persisted truth for the pair, an
  unrecognized token reading back as no saver, an invalid saver or
  out-of-range delay refused before any commit, applied/rejected/uncertain
  outcomes, first-baseline availability, occupied read-lane admission,
  same-lineage Applied readback, automatic stale-readback refetch and
  timeout-to-uncertain, unchanged-refresh diagnostic retention,
  owner replacement, busy write suppression, retry without replaying, the built-in
  choices and discovered sort order, and the mirror rules (confirmed saver
  reaches the lock screen, a refused commit never does, a saver with no
  scene hands the wallpaper back, a mirror failure is its own error).
- `qindaqt.settings-screensaver-lock-screen-saver-store`: taking the greeter
  wallpaper over, remembering exactly one displaced plugin and giving it
  back, `blank` keeping the plugin installed, and never moving a `[Daemon]`
  key.
- `qindaqt.settings-screensaver-preview`: every discovered token starts its
  own resolved program with the catalog arguments; `blank` opens the black
  full-screen window and closes on key, Escape, click, or pointer movement;
  denied activation closes Blank and reports failure; partial display failure
  closes windows already opened; the 60-second deadline closes a preview;
  `none`/unknown tokens refuse; a started program's nonzero exit and crash are
  surfaced; idle inhibition is acquired/released over a fake ScreenSaver D-Bus
  service, including Settings teardown; previews never stack; and the greeter
  is never resolved or invoked.
- `qindaqt.settings-screensaver-page`: the selector reflects truth and
  writes tokens, restores confirmed keyboard choices after refusal, shows
  no false selection before a baseline, disables delay for the built-ins,
  follows Preview availability, and writes the separate lock model only.
- The navigation rows pin the route appended after Startup applications
  ([ADR-0128](../adr/0128-accessibility-settings-route.md)).
