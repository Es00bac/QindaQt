# ADR-0169: report the program behind an opaque window class

- **Status:** Accepted
- **Date:** 2026-09-16
- **Owners:** Platform (compositor task facts) with Shell (iconography)
- **Supersedes:** None
- **Superseded by:** None

## Context

The compositor reports a window's application identity as its desktop file name
when the client declares one, otherwise its resource class
(`kwinshelltaskfacts.cpp`). For native applications that is exactly right.

For Windows programs under Wine or Proton it is not. Measured on the reporting
user's live session:

    WM_CLASS      = ("steam_app_0", "steam_app_0")
    _NET_WM_PID   = 132369
    _NET_WM_NAME  = "Battle.net"
    /proc/132369/cmdline
                  = C:\Program Files (x86)\Battle.net\Battle.net.exe --from-launcher

So the dock and task list showed **`steam_app_0`** with a placeholder icon.
`steam_app_<n>` is an artifact of how Steam and umu launch a title, not an
application identity; `steam_app_0` in particular is the non-Steam case and
identifies nothing at all. Wine's own shim classes (`explorer.exe`, `wine`,
`winemenubuilder.exe`) are equally uninformative.

The identity that *does* work is already present twice over: the client's
command line names `Battle.net.exe`, and the host's own `battlenet.desktop`
declares `StartupWMClass=battle.net.exe`. The shell's existing
`DesktopEntryIconResolver` matches `StartupWMClass` case-insensitively, so the
correct name and icon were one step away the whole time — the compositor was
simply reporting a key nothing could match.

## Decision

When, and only when, a window's reported resource class carries no identity — it
is empty, matches `steam_app_<digits>`, or is a known Wine shim — the compositor
reports the program actually behind the window instead: the basename of the last
`.exe` on the client's command line, or argv[0]'s basename for a native client.
Every other class is passed through untouched, because a real class is always
better identity than anything derived.

The policy is a pure function in `QindaQt::Compositor::foreignwindowidentity`,
so it is unit-tested without KWin or a live process. The PID it reads is
**KWin's authenticated client PID** (wl_client credentials, or XRes
`LOCAL_CLIENT_PID` on X11) — never the client-settable `_NET_WM_PID`, which
would let any window borrow another program's identity. The `/proc` read is
bounded and fails silently.

This stays **raw identity**: it answers "which program is behind this window",
never "what should the label say". All naming and icon policy remains in the
shell, which now resolves `Battle.net.exe` to `Name=Battle.net` and
`Icon=battlenet` through the desktop-entry lookup it already had.

One shell-side repair goes with it: `prettifiedApplicationId` treated any
dotted value as a reverse-DNS id, so `Battle.net.exe` became `Exe`. A Windows
executable name is a filename, so the `.exe` suffix is dropped and the rest kept
whole (`Battle.net`), while `org.qindaqt.Terminal` still yields `Terminal`.

## Consequences

- Wine and Proton programs appear in the dock and task list under the name and
  icon their launcher entry declares, instead of `steam_app_0`.
- A Windows program with no launcher entry at all reads as `RealGame` rather
  than `RealGame.exe` or `Exe`.
- Grouping improves as a side effect: two different Proton titles no longer
  share the identity `steam_app_0`, because each resolves to its own executable.
- A real Steam title still reports `steam_app_<n>` as its class and now resolves
  to its executable, which is an improvement but not the game's human name.
  Reading that from Steam's `appmanifest_<n>.acf` is a later refinement layered
  on this, not a replacement for it.
- The compositor now reads `/proc` for windows with opaque classes. It is
  bounded, authenticated, and skipped entirely for every ordinary window.

## Revisit when

- Steam appmanifest resolution lands (feed it the same `steam_app_<n>` key).
- Wine gains a way to declare a real desktop file name on its windows, at which
  point the desktop-file-name branch already takes precedence and this repair
  stops firing on its own.
