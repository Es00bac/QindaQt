# ADR-0227: The audio console on QindaTK

- **Status:** Accepted
- **Date:** 2026-09-21
- **Owners:** First-party applications (Settings)
- **Supersedes:** None
- **Superseded by:** None

## Context

The Settings Audio route's mixing console (ADR-0173) was rebuilt once already
for alignment and delegate lifetime (see the 2026-09-19 diagnosis in
[the route page](../apps/audio-settings.md)), but it remained a vertical form
of house `QindaQt.Controls` rows wearing console vocabulary: faders without a
printed scale, routing as list rows, no band structure shared between a strip
and a bus. The operator's direction is explicit — take Voicemeeter Potato as
the reference and take [ADR-0220](0220-rebuild-the-system-monitor-on-qindatk.md)'s
rebuilt System Monitor as the proof of what "right" looks like on this
desktop: dense, small chrome, real telemetry, QindaTK controls throughout.

QindaTK (dev-libs/qindatk) is the house toolkit for exactly this density:
`Tk.Flex`/`Tk.Grid` layout with CSS semantics, `Tk.Meter` with theme ramps,
and the `QindaTK.QindaQt` bridge that feeds the desktop's QST-1 tokens into
the toolkit theme so the route wears the same theme as the rest of Settings.
The service side already thinks in Voicemeeter terms — strips, buses, sends,
one gain law (ADR-0171), balance pan (ADR-0177) — so the model and protocol
were not the problem and are not changed here.

## Decision

The console's *presentation* is rebuilt on QindaTK; the projection, the Audio1
wire contract, the gain law, and the Settings1 keys are untouched.

Every input strip and every bus is a narrow desk card, and the cards flow
left-to-right and wrap — strips in one group, buses in another — with no
horizontal scroller anywhere in the surface. A card is six bands, shared
verbatim and in the same order between `AudioConsoleStrip.qml` and
`AudioConsoleBus.qml`: name; assignment (the device picker of ADR-0178);
control (pan dial plus mono/solo/mute lamps on a strip, the channel-mode
lamps of ADR-0180 plus mono/mute on a bus); routing (one send lamp per bus,
physical buses in the row above virtual ones, as the reference console prints
its A-row over its B-row); the desk (a live meter beside a fader with a
printed dB scale and a readout); and actions. A band that does not apply to a
card keeps its slot rather than collapsing, because a console is read across
a row and a ragged grid is a misread level; `qindaqt.settings-audio-console-page`'s
`consoleCardsShareOneGrid` fails if the two files drift apart.

Three controls the toolkit does not carry are built locally, each marked with
an `AGENT-NOTE` that it belongs in QindaTK: the vertical fader with its dB
scale (the toolkit's `Slider` is horizontal-only and carries no scale), the
rotary knob (the toolkit has no rotary control), and the desk lamp pad (the
toolkit's smallest button has form padding and no lamp semantics). They are
built on QtQuick.Templates with theme roles, the same construction QindaTK's
own controls use, and are the only new presentation vocabulary the route
adds.

The fader is driven by position and converted through the model's gain law;
the scale ticks, the numbered labels and the readout all call the model's own
`faderPositionForGain` / `gainForFaderPosition`, so a second mapping cannot
creep in beside ADR-0171's. Meters read the projection's `consoleLevels`
channel — published per strip and bus as `{peakDb, rmsDb, known}` — and an
unknown reading renders as an empty, dimmed meter, never a fabricated one.

Every fader, knob and pad is keyboard-reachable and adjustable, every strip
and bus is named to assistive technology, and no state is carried by colour
alone: the kind stripe on a card is paired with the physical/virtual word in
its name band, and lamp state is paired with the pad's pressed text.

## Consequences

The route's QML imports `QindaTK as Tk` and instantiates `QindaQtTheme` from
`QindaTK.QindaQt`; nothing links the toolkit's C++ API, matching the System
Monitor's contract — the module must be on Qt's QML import path, and the
build tree resolves it from the installed `dev-libs/qindatk`.

The shared band heights are a stated, tested contract between the two card
files; editing one without the other fails the page test, which is the
intent — the failure that motivated the rule came from exactly such a drift.

The local fader, knob and pad are deliberately the minimum. When QindaTK
grows a vertical fader with a scale, a rotary control, or a lamp button, this
route should adopt it and delete the local copy; the `AGENT-NOTE`s name that
obligation.

`tests/apps/settings/audio/visual_probe` renders the real page headless
against the duck-typed stub and grabs PNGs, so the mixer's density and scale
printing are reviewed as pictures, not inferred from a diff.

Documentation moved with the change: the route page's console section
describes the six-band grid, and the parity target stays in
[the reference page](../reference/voicemeeter-potato-parity.md).

## Revisit when

- QindaTK ships a vertical fader with scale markings, a rotary knob, or a
  lamp/pad control; the local copies then shrink the route's surface for no
  benefit and should be replaced.
- The Audio1 projection publishes per-strip level history; meters could then
  draw the short loudness trace the reference console draws, rather than the
  instantaneous peak/RMS pair.
- The Settings shell gains a detachable-panel host for routes; the console is
  the route that would benefit first, and the card grid's wrap model already
  assumes a freely-sized window.
