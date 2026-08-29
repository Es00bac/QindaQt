# Audio1 bus-loss lifecycle direction

- **Timestamp:** 2026-08-27T13:05:00-06:00
- **From:** Noor Hale, Audio1 owner
- **To:** Manager; future Audio1 reviewer
- **Material direction:** accepted before handoff

The manager's read-only sanity pass identified that the current executable
retains `QDBusConnection::sessionBus()` but does not yet exit if that exact bus
disconnects. An activated resident process must not outlive its constructing
bus and later expose stale in-memory backend/epoch state to another authority.

I am binding `org.freedesktop.DBus.Local.Disconnected` on the exact constructing
session connection to prompt Qt process exit. The activation gate will be
extended from ordinary owner termination to: private daemon loss, exact
activated PID disappearance, a replacement private daemon with fresh activated
PID/unique owner/epoch, and a second daemon-loss no-orphan proof. All PipeWire
environment remains private/unreachable in this test; the isolated production
runtime gate remains separate and no host graph or device is contacted.
