# ADR-0171: one gain law for the audio console

- **Status:** Accepted
- **Date:** 2026-09-16
- **Owners:** Platform (audio service) with Shell (audio surfaces)
- **Supersedes:** None (extends [ADR-0123](0123-voicemeeter-class-audio-graph-on-pipewire.md))
- **Superseded by:** None

## Context

ADR-0123 commits QindaQt to a Voicemeeter-class console over PipeWire
primitives. Audio1 v1 and v2 express level as a linear `double` scalar, which is
adequate for a single volume slider and inadequate for a console: an operator
reasons in **decibels**, reads numeric fader legends, and expects "-6 dB" to
mean the same loudness on an input strip, on an output bus, and on a matrix
send. Three surfaces each inventing their own scalar-to-dB conversion would
disagree in exactly the places a mixing engineer notices.

## Decision

One pure module, `QindaQt::Audio` gain (`audio_gain.h`), defines the whole law
and is shared by the service, the wire protocol and every console surface.

- **Range.** `-60 dB` to `+12 dB`, unity at `0 dB`. `-60` is the bottom of the
  printed scale and `+12` the makeup-gain headroom the reference console offers.
- **Bottom stop is exact silence.** `linearFromGainDb(-60)` is `0.0`, not
  `10^(-60/20) = 0.001`. A fader at its bottom stop must actually turn the
  source off; a near-zero linear value stays audible on a loud source and reads
  as a broken fader.
- **Fader taper.** Slider position maps to dB through a single power curve
  rather than linearly in dB, because a linear-in-dB fader spends most of its
  travel in a range nobody mixes in. Unity therefore lands high on the travel
  (between 0.6 and 0.95 of it), where a console puts it, and
  `unityFaderPosition()` is published so a surface can draw its 0 dB line
  without re-deriving the curve.
- **Round trip is exact.** Position → dB → position holds to 1e-9 across the
  whole travel, so dragging a fader and reading the value back cannot drift.
- **NaN fails quiet, infinities clamp.** NaN has no ordering and cannot be
  clamped, so it resolves to the bottom of the scale; a corrupt value must never
  arrive as full gain on the user's speakers. `+inf`/`-inf` clamp to the ends
  normally.

## Consequences

- Strip faders, bus faders and per-link matrix gains are one vocabulary, and a
  value moved between them means the same thing.
- The service converts to PipeWire's linear volumes at exactly one boundary.
- The taper is a product decision embedded in code. Changing the exponent
  changes the feel of every fader at once, which is the point; it must not be
  re-tuned per surface.

## Revisit when

- Per-channel trim or metering needs a different scale (a meter's dBFS scale is
  not this fader scale and should not reuse the taper).
- The console gains a fader whose printed range differs, at which point the
  range becomes a parameter rather than two constants.
