# ADR-0134: Own input and shortcut settings through KWin and kglobalaccel

- Status: Accepted
- Date: 2026-09-11

## Context

Settings had no way to change pointer, keyboard, or global shortcut behavior.
Settings1 schema v2 carries five `input.*` keys, but nothing consumes them.
KWin is the running authority for all of it:

- libinput device properties on `org.kde.KWin` at
  `/org/kde/KWin/InputDevice/<sysname>` (interface `org.kde.KWin.InputDevice`);
- key repeat and NumLock in `kcminputrc [Keyboard]`;
- keyboard layouts in `kxkbrc [Layout]`;
- global shortcuts in kglobalaccel, which runs inside KWin
  ([ADR-0009](0009-use-kglobalaccel-for-shell-shortcuts.md)).

A second copy of these values in Settings1 would drift from what KWin applies.

The gap-input-shortcuts worker was stopped before it finished. The Program
Manager completed the route and settled every mechanism below with private
headless KWin probes: the virtual backend on a private D-Bus session with
private XDG directories. The probe logs live under
`builds/qindaqt/gap-integration/proof/`.

## Decision

**Route.** Settings gains a twelfth built-in route, `input`, with Mouse &
touchpad, Keyboard, and Shortcuts tabs. Ctrl+1 through Ctrl+0 are taken, so
the route has no Ctrl+digit binding and is reached through the route list and
search. Its backend lives in its own QML module, so the Settings Center
composition root does not grow.

**Authorities.**

- Pointer and touchpad: the route sets device properties with
  `org.freedesktop.DBus.Properties.Set`. KWin applies and stores them itself;
  Plasma's mouse settings module does the same and writes no configuration
  file. Rows a device does not report as supported are hidden.
- Key repeat and NumLock: `kcminputrc [Keyboard]` keys `KeyRepeat`,
  `RepeatDelay`, `RepeatRate`, and `NumLock`.
- Layouts: `kxkbrc [Layout]` keys `Use=true`, `LayoutList`, and
  `VariantList`. A private KWin applied a layout list only together with
  `Use=true`.
- Global shortcuts: kglobalaccel's D-Bus API on `org.kde.kglobalaccel`.
- The Settings1 `input.*` keys are superseded and stay unconsumed. The schema
  does not change.

**Applying configuration files to a running KWin.** After writing `kcminputrc`
or `kxkbrc`, the route sends `org.kde.kconfig.notify` `ConfigChanged` on
`/kcminputrc` or `/kxkbrc` with the written group and keys. KConfig sends this
signal only for configurations opened by bare name, while the route opens
injected absolute paths so tests can relocate them; `kwriteconfig6` confirmed
that an absolute-path write announces nothing. In a private KWin, a runtime
layout change applied after the announcement, but not after the
`org.kde.keyboard /Layouts reloadConfig` signal or `org.kde.KWin /KWin
reconfigure`. The night light configuration port sends the same announcement
([ADR-0136](0136-night-light-through-kwin.md)). A write reports `Stored` only
when the announcement was sent and `org.kde.KWin` is on the bus; otherwise it
reports `StoredButReloadFailed`, and the page says the change waits for the
next session.

**Shortcut wire contract.**

- Listing uses `allMainComponents` (`aas`), then `getComponent` (`o`) and the
  component object's `allShortcutInfos` (`a(ssssssaiai)`) for active and
  default keys. `shortcutKeys` is not used: in a private KWin it returned no
  keys for actions whose keys were configured.
- Every reply's D-Bus signature is checked before decoding. `QDBusArgument`
  aborts the process when told to read a type the message does not carry, so a
  malformed reply fails closed instead of taking Settings down.
- Writes use `setForeignShortcutKeys` (`asa(ai)`) with the action id
  `[component unique, action unique, component friendly, action friendly]`.
  Each key sequence is the struct `(ai)` with exactly four ints.
  KF6GlobalAccel's `QKeySequence` decoder reads four ints without checking; a
  one-int sequence aborted a private KWin every time, with and without the
  QindaQt compositor plugin (gdb stops in
  `operator>>(QDBusArgument const&, QKeySequence&)` in `libKF6GlobalAccel`).
  The integer `setForeignShortcut` is not used.
- kglobalaccel answers success when it ignores an unknown action or keeps a key
  that another action holds. The port therefore reads the keys back and reports
  a refused assignment. "Assign anyway" first releases the key from every other
  holder, then assigns it.

**Custom command shortcuts.** A command is a desktop file
`<data home>/kglobalaccel/qindaqt-custom-<name>.desktop` with `Exec` and
`X-KDE-GlobalAccel-CommandShortcut=true`. The route registers it with
`doRegister([id, "_launch", name, name])` and binds keys with
`setForeignShortcutKeys`. In a private KWin, invoking `_launch` ran the command,
and `unregister(id, "_launch")` removed the binding. Only
`qindaqt-custom-*.desktop` components count as commands, so application
components such as `org.kde.konsole.desktop` are never removable from the
route.

The stopped worker's first shortcut backend sent the action id as
`[component, component, action, action]` with one int per sequence and decoded
replies as flat integer lists. Against the real daemon its writes changed
nothing, a four-int-unaware sequence would have aborted KWin, and its key
decoding could have aborted Settings. None of that shipped.

## Consequences

- Pointer settings, key repeat, layouts, shortcuts, and custom commands apply
  to the running session without a logout.
- The development machine has no touchpad or touch screen; touchpad rows are
  proven with fakes only.
- Pointer settings survive a login through KWin's own device configuration,
  not a QindaQt file. That path was not exercised on the live desktop.
- The `qindaqt.settings-input-*` rows run against fakes on private buses. The
  fake kglobalaccel refuses any sequence without four ints and the integer
  call, so a regression fails its row instead of reaching a compositor.
