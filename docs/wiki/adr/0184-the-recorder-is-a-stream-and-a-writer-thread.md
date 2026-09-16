# ADR-0184: the recorder is a stream and a writer thread

- **Status:** Accepted
- **Date:** 2026-09-16
- **Owners:** Platform (audio service)
- **Supersedes:** None (completes a gap named in [ADR-0173](0173-the-mixing-console-slice-of-audio1.md))
- **Superseded by:** None

## Context

The reference console records: any bus or strip to a file, from a transport
bar on the console. QindaQt had every bus on the graph and nothing that could
keep what went through it.

## Decision

### One recording, of one bus, to one file

`Recording { active, busId, path, startedAtMs }` rides on the console (schema
10). `StartRecording` (kind 25) names the bus and the format — `flac` or
`wav` — and `StopRecording` (26) needs nothing. A bus with no device is
refused (`bus-unbound`): a file of silence is worse than an honest refusal.
One at a time; a second start is `recording-busy`. `startedAtMs` is wall-clock
epoch milliseconds, so a surface shows elapsed time without the service
republishing every second.

Files go to `~/Music/QindaQt Recordings/<bus> <date time>.<format>` (or the
directory in `QINDAQT_AUDIO_RECORDING_DIR`), 48 kHz stereo, 24-bit PCM in FLAC
or WAV through libsndfile — a runtime dependency the package declares.

### A stream and a writer thread

The recorder is a `pw_stream` capturing the bus's device from its monitor,
non-passive because a recording is what the user asked for, and a writer
thread. The realtime callback never blocks and never touches the file: it
copies samples into a lock-free ring and returns. The writer drains the ring
at its own pace through `sf_writef_float`. If the ring ever fills — a stalled
disk — the realtime side drops audio and counts it; the file stays valid.

A write error stops the recording from the writer thread, which hands the
failure to the worker loop; the backend reports it as `recordingFailed`, and
the coordinator withdraws the recording and republishes, so the console never
shows a recording that is not happening. A recording declared before its
device is in the graph starts on the rebuild that brings the device; a
recording ends with the worker's core, its file complete.

### Surface

Every bus in the Settings console has a Record/Stop toggle bound to the
published state, so two surfaces agree on which bus is recording, and the
other buses' buttons are disabled while one records.

## Consequences

- Any bus — a physical one, or a virtual submix — can be recorded from the
  console, to a file the user can find.
- `media-libs/libsndfile` joins the install.
- The tray does not record; that is a Settings surface for now.

## Revisit when

- Recording a strip (pre-fader) is wanted; the same recorder with a strip's
  read node as its target.
- MP3 or Opus output is wanted; libsndfile writes Opus, lame writes MP3.
- A recording that spans a device change should follow it; today it stops
  with the device.
