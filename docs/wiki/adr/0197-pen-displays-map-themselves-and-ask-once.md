# ADR-0197: Pen displays map themselves and ask once

- **Status:** Accepted
- **Date:** 2026-09-17
- **Owners:** Session desktop controls; Settings Input route
- **Supersedes:** None
- **Superseded by:** None

## Context

A pen display is a screen and a tablet at the same time. KWin maps a tablet
tool with an empty `outputName` to whatever output is active, so plugging a
Wacom One 13 into the laptop leaves the pen drawing on the laptop's own panel
— the pen lands nowhere near the tip. Setting `outputName` to the pen
display's connector fixes it, and KWin persists that choice by output UUID in
`~/.config/kcminputrc`, so the repair survives re-plug and login.

Two things were missing. Nothing in QindaQt ever set `outputName`, and
nothing presented the rest of a tablet's settings: KWin 6.6.6 exposes the
whole surface a Wacom user expects (`outputName`, `outputArea`, `inputArea`,
`mapToWorkspace`, `calibrationMatrix`, `pressureCurve`,
`pressureRangeMin`/`Max`, `rotation`, `leftHanded`, `tabletToolIsRelative`,
pad button/ring/strip/dial counts, `deviceGroupId`, `size`) but the Input
route only listed pointers through `ListPointers`. KWin has no `ListTablets`;
the manager's `devicesSysNames` property plus the `tabletTool` / `tabletPad`
flags are the only enumeration, and `deviceAdded` / `deviceRemoved` are the
only hotplug signal. (Verified by introspecting
`org.kde.KWin.InputDeviceManager` and a device object on `qinda-top`,
2026-09-17.)

The risky part is not the writing; it is deciding *when* to write. A desktop
that keeps re-deciding where a pen belongs takes the pen out from under the
user's hand.

## Decision

**A tablet tool with no screen of its own is mapped automatically, exactly
once, and a recorded user choice is never overridden.**

**A tablet's identity is `vendor:product:name`, which is KWin's own.** This
is load-bearing enough to state first, because the rest of the decision is
worthless without it. KWin persists every per-device setting under
`group("Libinput").group(vendor).group(product).group(name)`
(`kwin-6.6.6 src/backends/libinput/connection.cpp:716`) — the
`[Libinput][1386][934][Wacom One Pen Display 13 Pen]` group in `kcminputrc`.
The ledger uses the same triple, with the trailing role word ("Pen", "Pad",
"Finger", "Touch") removed so a pen and its pad are one tablet.

KWin's `deviceGroupId` is **not** that identity and must never key anything
that outlives a session: it is a base64 SHA-1 of the libinput device group's
*pointer address*
(`src/backends/libinput/device.cpp:450`, hashing `asprintf("%p", group)`).
It is a correct way to pair a pen with its pad while both are plugged in, and
a silent way to break "the mapping survives a re-plug" if a record is keyed
on it — a re-plug makes a new group object, the tablet reads as new, it
announces again, and the user's recorded choice is never found. A row exists
whose only purpose is to fail if the key ever regresses to it.

Two identical tablets of the same model share this identity, exactly as they
share KWin's own configuration group. libinput exposes no serial through
KWin's D-Bus surface, so that limit is inherited rather than chosen.
Records written under the old pointer-hash key are **dropped** when the
ledger is read: they can never match a live device again, so keeping them
would leave permanent phantoms in the ledger and in the Settings list. The
cost is one extra notification for a tablet that had already been announced.

- The session process (`qindaqt-desktop-controls`) owns the decision. It
  subscribes to `deviceAdded` / `deviceRemoved`, watches the screens it
  already has, and reconciles every tablet on start, on hotplug, and whenever
  the set of outputs changes. A pen plugged in before its HDMI output is
  therefore mapped when the output arrives, not left behind.
- An output is a pen display's own screen when its EDID manufacturer is a
  display-tablet vendor (`WAC`/Wacom, Huion, XP-Pen/UGEE, matched against both
  the raw PNP code and KWin's decoded vendor string), or, failing that, when
  its model or label shares a distinctive word with the tablet's product name.
  **An internal panel is never a match**, because the laptop's own screen is
  the exact wrong answer this feature exists to remove. Two equally good
  candidates are ambiguous: the policy writes nothing and the user chooses.
- Every decision is recorded per tablet identity in one Settings1 key. A
  record carries `userChosen`; once it is true, automatic matching never
  replaces it, on this or any later plug-in.
- **Both writers share one store.** The session process decides mappings and
  the Settings route records what the user chose, through the same
  `TabletMappingStore` implementation in the shared library. A route that
  wrote only to KWin would have its choice re-decided by the session policy
  on its next pass and silently reverted, which reads to the user as
  "Settings does nothing".
- **A tablet this desktop has not recorded, but that KWin already maps, keeps
  that mapping.** It may be correct — from KWin's own persisted
  `OutputUuid`, or from another tool — and clearing it to the active screen
  would un-map a working pen. Only a tablet with no mapping at all gets the
  documented default.
  Until the ledger has actually been read, the policy acts on nothing, because
  acting could override a choice it has not seen.
- The user is told once: a notification names the tablet, says which screen
  the pen now draws on, and offers **Set up pen display**, **Use the active
  screen instead** and **Not now**. A re-plug of a known tablet is silent. The
  one exception is a mapping that genuinely changed — the USB-before-HDMI
  case, where the first pass could only say "it follows the active screen".
- Everything else a tablet can do lives in Settings → Input → **Pen &
  tablet**, one destination per tablet (not per device), with every control
  hidden when the device's own `supports*` flag is false (ADR-0134).

**The ledger is one Settings1 `object` key, `input.tabletMappings`**, whose
members are device group ids. Settings1's schema is a closed list of keys
(ADR-0126), so a key per device cannot exist; a single document keyed by
device group is the only representable shape, and it is read and written
through a client scoped to exactly that key.

**Tablet identity and output identity are one shared boundary.**
`QindaQt::TabletDevices` holds the KWin port, the hotplug watcher, the output
matcher, the ledger and the calibration/area geometry. The session process and
the Settings route both use it, so the screen the session maps a pen to and
the screen the Display card badges as a pen display can never disagree.

## Consequences

- The session process gains a second purpose-scoped Settings1 client. Widening
  the existing idle client's scope instead would put idle policy behind this
  feature's schema risk, because Settings1 rejects a whole snapshot on one
  unknown key.
- `input.tabletMappings` is new in `data/settings/schema-v2.json`. The resident
  settings service must be restarted (a re-login) before it will accept the
  key; until then the store stays unloaded and the policy declines to act,
  which is the safe direction.
- Output identity in both processes comes from `QScreen` — KWin fills a
  Wayland output's make/model from the EDID and Qt republishes them, with
  `name()` being the connector that `outputName` takes. Neither process needs
  a Display1 client for a decision that needs one string per output.
- Calibration runs **on the screen the pen is mapped to**, in its own
  full-screen window, and measures the **stylus** rather than the cursor.
  Calibrating inside the Settings window would measure whichever screen that
  window sits on, and accepting mouse events would let a stray touchpad tap
  become a sample. A tablet that follows the active screen has no fixed
  surface to calibrate against, so the wizard is unavailable rather than
  wrong. It is measured with the device's own calibration in force: the
  wizard resets first, then fits the four measured points onto the four
  targets. Measuring through an existing matrix would compose two corrections
  and drift further every pass. A degenerate measurement is refused rather
  than written, because a collapsed matrix loses the pen entirely.
- libwacom stays optional and unused. The EDID/name heuristic plus KWin's own
  capability flags decide, and an opaque tablet with no match keeps KWin's
  default.
- The Display route gains a read-only "Pen display" badge and a link into the
  tablet route; it never writes tablet state.

## Revisit when

- KWin gains a `ListTablets` method, a tablet-specific object path, or a
  binding interface for pad buttons — the route currently sends pad bindings
  to global shortcuts because KWin exposes counts and no bindings.
- Settings1 gains per-key namespaces, which would let each device group own a
  record instead of sharing one document.
- A display tablet ships with an EDID this table does not recognize and a
  product name that shares no distinctive word with its output.
