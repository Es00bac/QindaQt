# ADR-0185: VBAN is a document and two threads

- **Status:** Superseded
- **Date:** 2026-09-16
- **Owners:** Platform (audio service)
- **Supersedes:** None (completes the last graph gap named in [ADR-0173](0173-the-mixing-console-slice-of-audio1.md))
- **Superseded by:** [ADR-0246](0246-configure-manual-audio-peers-through-audio1.md)

## Context

VBAN is the reference console's network audio: an open UDP protocol, a
28-byte header and interleaved PCM, that carries a bus to another machine or
brings a remote stream in as an input. The host had no VBAN implementation
at all, in Portage or elsewhere; the protocol is small enough to speak
in-tree.

The Audio route's intent surface is closed — its boundary gate refuses any
text field — so a host name or a stream name cannot be typed into Settings.

## Decision

### Streams are a document; Settings holds the switches

Streams are defined in `$XDG_CONFIG_HOME/qindaqt/audio-vban.json`, read
fail-closed like macro buttons — see [Audio VBAN streams](../reference/audio-vban-streams.md).
An outgoing stream names a bus, a host and a port; an incoming one names the
stream to accept and the port to listen on. The console publishes every
defined stream (`Console::vban`, schema 11) with two bits: `enabled`, the
user's switch, persisted with the console; and `active`, whether the graph
carries it right now. `SetVbanEnabled` (kind 27) is the one operation, and
Settings shows one switch per stream.

An enabled outgoing stream is declared to the graph only when its bus has a
device — switched on, not running, is an honest state and reads as one.

### The wire format is one pure file

`vban_packet` is the whole wire format: header encode and decode, verified
against the reference layout — magic, the 48 kHz rate index, counts minus
one, PCM16 or float32, the 16-byte name, the little-endian frame counter —
and refusing anything else: a foreign magic, another rate, an integer type we
do not take, a payload that does not match its header. Nothing else in the
service knows a byte of VBAN.

### Two threads, one ring each

A sender captures the bus's device from its monitor into a lock-free ring;
its own thread packetises 256-frame stereo PCM16 — the reference default —
and sends. A receiver's thread blocks in `poll`/`recv`, decodes, folds any
channel count to stereo and pushes into a ring; a playback `pw_stream`
presented as an `Audio/Source` virtual source, `qindaqt.vban.<name>`, pulls
from it. The realtime callbacks touch the rings and nothing else. A late or
lost packet is silence, never a stall: the receiver primes on a quarter
packet of margin and drops back to silence when it runs dry.

## Consequences

- A bus can be sent to another machine and a remote stream received as a
  source the strips can read, with the reference console's own protocol.
- Only the audio sub-protocol; MIDI, serial and text sub-protocols are not
  spoken. Only 48 kHz; a sender or receiver at another rate is ignored.
- The user edits a file to define streams; the closed intent surface stays
  closed.

## Revisit when

- Other sample rates, or resampling on receive, are wanted.
- A stream definition surface outside the Audio route's closed gate exists.
