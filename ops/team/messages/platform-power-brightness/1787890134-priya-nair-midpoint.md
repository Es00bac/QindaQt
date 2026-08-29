# Midpoint: power/brightness lane resumed; material decisions reached

- Worker: Priya Nair
- Timestamp: 2026-08-28T04:08:54Z (2026-08-27 22:08 MDT)
- Acknowledgment: my earlier supersession message in this thread remains
  in place unedited. The lane was resumed by manager instruction; Rhea Calder
  is the receiving platform/display lead and the manager owns sequencing.

State: repository reading, board refresh (including the D0/D1 threads, the
new outcome queue, and the lead-assistant trial), and read-only upstream
documentation research are complete. The complete handoff is being written
now. No product file has been edited and no live host power, battery,
backlight, DDC, logind, inhibitor, or settings state has been inspected.

Material decisions reached (full contracts, evidence, and alternatives in the
forthcoming handoff):

1. Power1 exists as one bounded resident D-Bus service; Brightness1 does not
   exist as a process. Brightness composition is a pure consumer-side model;
   its internal-panel and external-monitor mutation transport is the
   compositor through Display1's typed class-B policy surface; keyboard
   backlight transport is UPower inside Power1.
2. Session power actions (suspend, hibernate, reboot, power off) are called
   by the shell, not by Power1, because the logind polkit actions authorize
   the calling process's session state and an out-of-session user service
   would be treated as inactive. Power1 publishes availability truth; the
   shell is the mutating session subject. This amends the earlier
   platform-services plan's Power1 action placement, with reasoning.
3. Ambient/adaptive brightness policy is QindaQt-owned (claim/release of the
   sensor proxy light interface, injected sensor port, pure curve model),
   driving the compositor class-B surface, with an explicit never-both rule
   against the compositor's own auto-brightness flag.
4. The pinned plasma-wayland-protocol device/management XMLs were read
   directly this session: they natively carry class-B brightness, dimming,
   SDR brightness, ddc-ci-allowed, and auto-brightness state and capability
   bits, plus a failure-reason event, which substantiates the accepted
   Display decision's class-B transport with an evidence path for its
   device-error-truth condition. KWin 6.6.5 builds contain no ddcutil
   linkage and no ddc backend directory, so the DDC/CI hardware transport is
   compositor-internal and optional at runtime; QindaQt never touches i2c or
   sysfs directly.
5. v1 lid behavior is upstream logind defaults with no QindaQt override; the
   handle-lid-switch inhibitor path is reserved for a later slice.
6. Lock-before-suspend is a shell-owned sleep delay inhibitor with
   PrepareForSleep sequencing; Power1 mirrors preparing-for-sleep state.
7. v1 stores no per-display brightness restore in Settings1; keys are
   proposed for a later Settings-owned slice.
