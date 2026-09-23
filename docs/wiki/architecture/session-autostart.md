# Session-owned XDG autostart

The session supervisor owns one XDG autostart batch per login. This is
[ADR-0247](../adr/0247-run-xdg-autostart-in-the-session-supervisor.md).
After activation environment publication, notification host and shell startup,
and named optional desktop children start, the supervisor scans XDG autostart
directories on the first event-loop turn after Session1 and session bridges
register. It scans once. Stopping before that turn cancels launch. Shell replacement does not rescan or relaunch entries.
Logout, essential-child failure, and supervisor destruction terminate direct
children that batch still owns. A short-lived entry is not restarted.

The read-only src/session_autostart catalog takes explicit user/system
directories, XDG_CURRENT_DESKTOP names, executable search paths, and a
terminal executable. Production qindaqt-session resolves these from XDG.
The desktop launcher sets XDG_CURRENT_DESKTOP=QindaQt. Empty roots in the
public supervisor options run no general autostarts. The
qindaqt-session --no-autostart flag is for private and diagnostic sessions.

A user file shadows the same basename in every system directory, even when
hidden or malformed. An Application entry with a Name and bounded valid Exec
is eligible only when Hidden and X-GNOME-Autostart-enabled allow it,
OnlyShowIn/NotShowIn match, TryExec and the command resolve, and any Path
exists. The catalog decodes desktop-entry scalar escapes in Name, Comment,
Icon, and TryExec before display or field-code expansion; malformed values
remain visible as ineligible. The public launcher parser alone decodes Exec
and Path and expands Exec to argv without a shell.
Terminal=true uses qqterm -e. Unsupported D-Bus activation and
non-Application GNOME startup phases remain visible in Settings with reasons
and do not launch.

[Startup Settings](../apps/startup-settings.md) reads this catalog and only
writes user overrides. Its Enabled switch describes next-login configuration;
an ineligibility line explains why a configured entry will not execute.
Re-enabling clears both recognized disable flags. A03 installs one stable
system `qindaqt-obs-login.desktop` entry with `OnlyShowIn=QindaQt;` through
this same runner. The [OBS login helper](../adr/0248-confirm-streaming-preferences-before-obs-consumption.md)
reads the confirmed Settings1 preference and replaces its own PID with OBS
only when enabled. A user Hidden override in Startup can mask that system
entry; Streaming settings reports the effective login availability.

The supervisor owns only direct processes it spawned. It does not kill an
ambient instance with the same executable name, adopt daemonized descendants,
or persist run history.

Focused gates use disposable XDG roots and a private D-Bus session. See the
[Startup Settings verification](../apps/startup-settings.md#verification).
