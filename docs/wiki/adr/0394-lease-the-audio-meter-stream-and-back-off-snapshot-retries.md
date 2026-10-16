# ADR-0394: Lease the audio meter stream and back off snapshot retries

- Status: Accepted
- Date: 2026-10-16

## Context

`org.qindaqt.Audio1.Levels` is the one high-rate signal on the session bus:
twenty frames a second, decoded and queued for every subscriber. `AudioClient`
installed its match rule when it started, so every client, the shell, a Settings
window parked on another page, and the OBS bridge, received and decoded every
frame whether or not a meter was on screen. Separately, the OBS audio client
retried a refused or contradicted snapshot every 200 ms for the life of the OBS
process.

A Settings process that had been open on the Appearance page for about 26 hours
stopped reading its session-bus socket while the daemon queued about 4 GB for it
(about 43 KB/s, which is the order of the Levels stream). The mechanism was
reproduced with a standalone harness: QtDBus delivers a signal to a slot on
another thread by queueing the `QDBusMessage`, which retains the libdbus
message; libdbus stops watching the socket for reads once the live messages of a
connection pass its limit (63 MiB); the watch is switched back on from whichever
thread frees the message, which for queued deliveries is the GUI thread, while
Qt's `QSocketNotifier` lives in the D-Bus worker thread, and
`QSocketNotifier::setEnabled` from another thread only warns and does nothing.
The read notifier is then gone for good: the process is alive, its event loop
runs, and it never reads again. The harness reproduced exactly that (a flood
delivered, then a trickle never received, plus the Qt warning). What the
harness cannot show is what stalled the GUI thread in the field; the stream
volume is consistent with Levels but the trigger is inferred, not observed. The
failure is in Qt and cannot be repaired here, so the defense is not to be a
subscriber of a high-rate stream without a reason.

## Decision

- **Meter lease.** `AudioClient::retainLevels()` and `releaseLevels()` are a
  counted lease (`LevelsLease` is the RAII form). The first lease sets the
  transport's standing want (`AudioTransport::setLevelsSubscribed(true)`), which
  installs the `Levels` match rule for the current owner and for every later
  owner and survives `stop()`/`start()`; the last release removes the rule,
  clears every retained reading and emits an empty `levelsChanged` batch (the
  same "metering stopped" signal an upstream stop gives), and a frame already
  queued is dropped. `Changed` stays subscribed regardless. A release without a
  lease is ignored.
- **Who holds a lease.** A meter holds one only while it is visible in a window
  that can show it: QML meters (`AudioConsoleMeter`, `AudioLedMeter`) lease from
  their owner (`AudioSettingsModel`, `AudioAppletController`) while visible in a
  shown, non-minimized window; the OBS dock leases from its bridge while shown.
  The owners release every lease they still hold when destroyed. Read denial
  grants the shell controller no lease.
- **Backoff.** A refused, lost or contradicted snapshot is retried after
  200 ms, doubling to a ceiling of 10 s (`RetryBackoff`, deterministic, no
  jitter); any healthy answer, a new owner or `stop()` resets it.

## Consequences

An idle desktop receives no meter traffic at all. The shell tray popup, whose
meters used to be wired to a signal nobody emitted, now animates while it is
open. Tests: `qindaqt.audio-client-levels` (lease counting, reapplication across
owners and restarts, idle batch, stale-frame drop, backoff schedule and reset
driven through the retry timer without sleeping), `qindaqt.audio-qt-transport`
(match rule installed and removed on a private bus), the Settings, shell applet
and OBS dock lease rows.

**Why the service keeps metering.** Audio1 cannot see the match rules of its
subscribers, so it cannot know that nobody listens, and stopping its meter
tap without a protocol addition would silence meters for other clients. The
service-side fix is the `AcquireMeters`/`ReleaseMeters` pair already named in
[ADR-0174](0174-meters-are-a-stream-not-a-snapshot.md) under "Revisit
when"; it is a public contract change and is not made here. Until it lands, the
service still produces the frames and the daemon discards them for lack of
matching subscribers.

## Revisit when

The Audio1 contract gains explicit meter acquisition, at which point the lease
count should drive the service as well, or Qt fixes cross-thread watch toggling.
