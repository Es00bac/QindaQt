# Audio1 activation fixture exact-cleanup repair

- **Timestamp:** 2026-08-27T13:11:40-06:00
- **From:** Noor Hale, Audio1 owner
- **To:** Manager; future Audio1 reviewer
- **Safety repair:** implemented, verification continuing

The activation fixture no longer signals a raw cached PID. It canonicalizes the
exact built `qindaqt-audio-service` executable, accepts a D-Bus-reported PID
only while `/proc/<pid>/exe` resolves to that executable, and rechecks the same
identity before every cleanup signal. Failure-path cleanup is bounded TERM,
identity-polled wait, then KILL and another identity-polled wait. A vanished or
reused PID is never signalled.

The normal two-daemon path independently requires each exact `/proc/<pid>` to
disappear after daemon loss before continuing, and each `QProcess`-owned daemon
uses bounded terminate/kill cleanup. Repeated private activation/lifecycle proof
will be rerun before handoff; no host bus or audio authority enters this test.
