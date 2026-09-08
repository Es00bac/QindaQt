# ADR-0108: Compose System Monitor from detachable views

- **Status:** Accepted
- **Date:** 2026-09-08
- **Owners:** First-party applications
- **Supersedes:** None
- **Superseded by:** None

## Context

QindaQt needs a complete desktop system monitor. Users need both a quick answer
about a slow machine and detailed process, resource, and hardware information.
The desktop already supplies window containers, recursive splits, and tabs.
Duplicating that machinery inside a monitoring application would produce two
competing ways to arrange the same work.

## Decision

[System Monitor](../apps/system-monitor.md) is an ordinary Qt Widgets application.
A main window offers Overview, Processes, CPU, Memory, Disks, Network, and
Hardware views. Any view can open in an independent window. These windows use
normal compositor decorations and application identity; QindaQt containers own
tiling and tabbing. Windows in one process share a sampler and bounded history.
A command-line view selector supports launching a particular view directly.
Saved-workspace window reassignment continues to use the existing compositor
workflow; the app does not invent a second workspace-restoration protocol.

Keep Linux sampling, process actions, snapshot/history models, hardware providers,
and presentation separate. Collect counters outside the GUI thread. Rates use
elapsed monotonic time and counter deltas; reset, first-sample, and disappearing
devices must not produce fabricated activity. Display unavailable readings as
unavailable. CPU process percentages use a documented, consistent denominator.

Read Linux procfs and sysfs directly. Optional vendor GPU telemetry stays behind
the hardware provider and does not become a required proprietary dependency.
Process actions target the selected process identity, including its start time,
and report the real operating-system result. Stale selected identities are
rejected before an action. Signals use pidfds; systems without this support
report an unavailable identity handle. Priority changes use immediate identity
checks; Linux does not offer an atomic start-time condition for this numeric-PID
operation, so priority changes remain best-effort.

Use QindaQt's public appearance and application-menu boundaries. Charts are
painted using Qt's existing rendering facilities, avoiding a separate charting
runtime. Dense tables, contextual controls, keyboard navigation, visible units,
and useful compact windows take priority over decorative dashboard cards.

## Consequences

The same view can be used alone, alongside another application, or in a saved
monitoring workspace. The application does not call compositor-private APIs.
A separate process has its own sampler; view windows created within the same
process share one. Hardware coverage depends on the running kernel and driver,
and missing sensor or GPU capabilities remain visible in the interface.

Focused verification covers counter deltas, process identity/actions, hardware
parsing, graph/table interaction, and independent view lifetime. Installed
acceptance exercises live values and disposable-process actions. Gentoo package
metadata must name build/runtime dependencies and install the launcher and app
through Portage.

## Revisit when

A measured sampling cost across independent application processes warrants a
shared monitoring service, or a supported driver requires a new telemetry API.
Neither possibility justifies a resident daemon before that need is measured.

## Gentoo ownership

The initial monitor is delivered as `gui-apps/qindaqt-system-monitor`. Its
runtime dependency on `gui-wm/qindaqt-desktop` supplies the common QindaQt
libraries; the SystemMonitor install component contains only the new app,
launcher, and icon. This avoids competing ownership of shared libraries or a
compositor replacement for an app-only installation. NVIDIA telemetry loads
NVML only when available from the existing driver installation.

A future desktop recipe that includes System Monitor files must replace/block
the separate app package, or exclude this component and keep app packaging
separate. It must not install the same paths from two packages.
