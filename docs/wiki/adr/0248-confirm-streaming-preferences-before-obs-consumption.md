# ADR-0248: Confirm Streaming preferences before OBS consumption

- **Status:** Accepted
- **Date:** 2026-09-23
- **Owners:** Streaming preferences, Settings Streaming, OBS applet, OBS login helper
- **Supersedes:** None
- **Superseded by:** None

## Context

Settings1 commits are asynchronous. The former Streaming adapter published
requested values before Settings1 accepted them. Rejected writes therefore
looked saved in Settings; the shell applet independently used OBS's config
port and connected even when `services.obsAutoConnect` was false. The
`services.obsStartAtLogin` setting had no execution consumer. [ADR-0247](0247-run-xdg-autostart-in-the-session-supervisor.md)
now gives QindaQt one XDG autostart runner.

## Decision

`src/services/streaming_preferences` is the public, same-thread preference
boundary shared by Settings and shell. It scopes its Settings1 client to the
three OBS keys. It publishes values only from an exact-owner confirmed
snapshot. A setter accepts one pending asynchronous request, with no
optimistic value change. A confirmed commit awaits authoritative readback;
rejection leaves the previous values and exposes its message. Timeout or
owner replacement is uncertain, never replayed. Additional writes while one
is pending are refused rather than queued.

Neither Settings nor shell automatically connects before a confirmed baseline.
The applet connects only while auto-connect is true, OBS's websocket config is
present and its active port equals the selected Settings1 port, and the
existing scoped secret can be read. It keeps watching setup and preferences
throughout the session. Settings supports explicit manual Connect when
auto-connect is false. Changing the selected port does not reconfigure a
running OBS; the Setup/Repair step writes OBS's config and OBS must restart
to read it.

One system `qindaqt-obs-login.desktop` entry under `/etc/xdg/autostart`
uses `OnlyShowIn=QindaQt;` and `Exec=qindaqt-obs-login`. A02 is its sole
session launcher. The helper waits at most five seconds for an exact-owner
Settings1 baseline, exits without OBS if unavailable or disabled, and `exec`s
`obs --minimize-to-tray` in its own PID when enabled. It never reads the OBS
secret, opens obs-websocket, or starts a detached child. A user Hidden override
in Startup masks the system entry under A02's normal basename rule. Streaming
Settings reports this effective entry status instead of claiming that a true
preference alone guarantees a login start.

## Consequences

The desktop package must install the helper into the session PATH and the
single system entry into an XDG config directory searched by A02. A custom
`XDG_CONFIG_DIRS` that excludes `/etc/xdg` must include its installed entry
elsewhere. The shell applet and Settings each own a scoped Settings1 client instance
but share one preference policy and OBS client implementation; no UI module
is linked into the shell or session. A Settings1 outage at login fails closed
for OBS launch. Tests use private Settings1/OBS processes and disposable XDG
roots; they never execute ambient OBS or user autostarts.
