# ADR-0100: Own desktop essentials in a session process

- **Status:** Accepted
- **Date:** 2026-09-07
- **Owners:** Session supervision and platform
- **Supersedes:** None
- **Superseded by:** None

## Context

Daily desktop essentials — media-key volume/mute and brightness with visible
feedback, a functional Print screenshot action, a session-started polkit
authentication agent, and a configurable idle display-off policy — need homes
in a desktop whose shell, compositor, and services are strictly separated.
Three candidate owners existed:

1. The KWin integration plugin (compositor process): key dispatch would be
   trivial there, but mixing desktop policy into the compositor duplicates the
   QindaQt service layer and widens the release-matched binary's blast radius.
2. The shell process: it already links KGlobalAccel, but the essentials are
   not presentation — the shell would gain audio, backlight, screenshot, idle,
   and DPMS responsibilities that must survive shell restarts and remain
   usable by a minimal session.
3. A separate bounded session process supervised by `qindaqt-session`:
   lifetime tied to the session, no compositor or shell coupling, free to
   compose the existing public clients and native session services.

The audit also fixed the available primitives: KWin 6.6.6 provides
`org.kde.kglobalaccel`, `org.kde.KWin.ScreenShot2`, and org-kde-kwin-dpms;
KIdleTime's Wayland backend drives ext-idle-notify-v1; the resident
`org.qindaqt.Audio1` exposes volume/mute through the public `AudioClient`;
and the power service's public `SysfsBacklightSource` carries ADR-0060's
tested direct-sysfs write primitive, which Power1 v1 deliberately does not
expose as a wire method.

## Decision

Implement the essentials in one bounded `qindaqt-desktop-controls` process
(`src/session/desktop_controls`), started by the `qindaqt-session` supervisor
as an optional one-restart child after the shell.

- Media keys register through KGlobalAccel Autoloading with stable action
  ids; user remapping survives restarts and no binding is ever stolen.
- Volume and mute dispatch through the public `AudioClient` on the default
  output; brightness through the public `SysfsBacklightSource` write
  primitive. No Power1 protocol change is made; the later PB-4/PB-5 method
  can supersede the direct primitive without moving the key handlers.
- Print launches `spectacle -b -r` detached; QindaQt does not reimplement
  screenshot capture or bypass any screenshot permission.
- Visible feedback is one replaceable `org.freedesktop.Notifications` popup
  per category; no new OSD surface or shell dependency is introduced.
- Idle display-off uses KIdleTime plus org-kde-kwin-dpms on a private Wayland
  connection; it never locks the session and leaves the screen-lock
  preference untouched. The preference is the purpose-scoped Settings1 key
  `power.idleDisplayOffMinutes` (-1 = never, 1..240 minutes, default 10).
  Inhibition is claimed only for native Wayland surface inhibitors
  (ext-idle-notify honors them); `org.freedesktop.ScreenSaver`/portal-only
  inhibition is an explicit non-claim until a suppression path or nested
  runtime evidence lands. A DPMS controller that becomes available after
  policy start re-applies the current preference, so late binds cannot leave
  the policy permanently passive.
- The supervisor also starts an optional polkit authentication agent from
  well-known distribution paths; absence is honest and non-fatal.

## Consequences

- The essentials keep working across a shell crash and consume no compositor
  build slot; a compositor restart ends the session anyway, which is the
  intended lifetime.
- Brightness writes happen in a second process that binds the same public
  sysfs primitive as the power service's inventory; the service performs no
  writes today, so there is no concurrent-writer protocol until Power1 gains
  a method, at which point this process should switch to it.
- The Settings Power route's public-import allowlist gains the
  `qindaqt/services/settings_client/` and `qindaqt/session/desktop_controls/`
  prefixes for the idle section; the boundary's substring include-suffix
  heuristic means repository filenames must never equal a standard header
  name with a suffix (the supervisor helper is named
  `session_optional_child.*` for exactly this reason).
- Packaging must carry spectacle, a polkit authentication agent, and the
  KGlobalAccel/KIdleTime/KWayland client libraries as session dependencies;
  builds without those CMake configs degrade to no resident process while
  the library and tests still build.
- Physical media keys, backlight writes, Spectacle launches, polkit prompts,
  and live idle/DPMS cycling remain installed-session evidence, not claims of
  this code's focused gates.
- D-Bus-only idle inhibition (`org.freedesktop.ScreenSaver.Inhibit`, portal
  `Inhibit` flag 8) is not consumed; a later slice must either observe that
  state before requesting DPMS off or produce nested runtime evidence
  bounding the exposure before any general inhibitor-respect claim.
