# ADR-0177: pan is balance on the send

- **Status:** Accepted
- **Date:** 2026-09-16
- **Owners:** Platform (audio service)
- **Supersedes:** None (completes a gap named in [ADR-0173](0173-the-mixing-console-slice-of-audio1.md))
- **Superseded by:** None

## Context

`Strip::pan` was on the wire, validated, drawn and persisted, and changed
nothing a listener could hear. The reference console pans every strip; a pan
control that only moves a number is the kind of feature the request was
explicit about not wanting.

## Decision

### Where it is applied

On the **send**. Each enabled matrix cell is a loopback whose playback node's
volume is that cell's gain ([ADR-0173](0173-the-mixing-console-slice-of-audio1.md));
pan is that same node's two channel volumes set differently. Every send out of
a strip is panned identically, which is what a strip pan means, and the
routing edge carries the strip's pan so the backend never has to look the
strip up.

The send's playback side is now **always stereo** (`audio.position = [ FL FR ]`),
whatever the source. A mono microphone is upmixed inside the loopback by its
own channel mixer, so there are two channels to pan on; before this the send
inherited the source's layout and a mono strip would have had nothing to pan.

### The law

Balance, not constant power: the centre is unity on both sides, and the control
attenuates the far side only, linearly, reaching silence at the stop. The
console's fader legend says 0 dB; a constant-power law would put both sides at
−3 dB the moment a strip existed, and the legend would lie for every strip
nobody had panned. NaN reads as centre; beyond the stops is the stop, so no
gain outside [0, 1] can reach the graph.

### The wire to the graph

The mixer API's `set-volume` takes per-channel state as
`{channelVolumes: a{sv}}` keyed by decimal channel index with nested
`{"volume": d}` — the same shape S1's `SetChannelVolumes` already uses. Because
the send is always stereo the keys are exactly `"0"` and `"1"`, and each is the
cell's linear gain times that side's balance factor. A muted or solo-silenced
edge is still carried at zero on both.

## Consequences

- Pan is audible, per strip, on every bus the strip feeds.
- A mono source pans the same way a stereo one does.
- The pan law lives beside the gain law in `audio_gain.h`, tested at the
  stops, the centre, past the stops and at NaN, and for monotonicity.

## Revisit when

- Intellipan (a second axis for surround buses) is built; the send's channel
  layout would follow the bus rather than being fixed stereo.
- Per-channel trim lands; it belongs *before* the fader, on the capture side,
  not in the send's playback volumes.
