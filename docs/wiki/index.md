# QindaQt project wiki

QindaQt is a modular, Wayland-first Qt desktop environment. Its distinguishing
feature is a hybrid window model: ordinary windows remain independently usable,
but users may combine them into a movable container containing tabs and
recursive splits.

This wiki is the canonical source for architecture, behavior, and contributor
workflow. It changes with the code and is validated as part of the repository.

## Start here

- [Architecture overview](architecture/overview.md) describes the runtime and
  its process boundaries.
- [Module boundaries](architecture/module-boundaries.md) defines ownership and
  dependency direction.
- [Compositor and session integration](architecture/compositor-session.md)
  records the exact KWin ABI, launcher/plugin boundary, completed Compositor
  MVP evidence, and explicit later-milestone boundaries.
- [Notification service](architecture/notifications-service.md) records the
  bounded model, freedesktop adapter, authenticated host/client transport,
  descriptor provisioning, and remaining service policy.
- [Notification presentation](shell/notification-presentation.md) records the
  bounded production popup/center behavior and its unqualified boundaries.
- [Window containers](architecture/window-containers.md) specifies the core
  domain model and mutation invariants.
- [Hybrid topology](architecture/hybrid-topology.md),
  [constraints and restore state](architecture/hybrid-constraints.md), and
  [container chrome](architecture/hybrid-chrome.md) define the implemented
  process-local interaction architecture, evidence, and later boundaries.
- [Layout profiles](shell/layout-profiles.md) explains how QindaQt can present
  QindaQt, GNOME-, Unity-, MATE-, XFCE-, NeXTSTEP-, macOS-, Windows-inspired,
  and user-created workflows without separate shells.
- [Production panel surfaces](shell/panel-surfaces.md) defines the real
  LayerShellQt-backed panel runtime and its current qualification boundary.
- [Panel visibility policy](shell/panel-visibility.md) defines window-aware
  hiding, reveal/hold priority, and reservation intent without platform side
  effects.
- [Applet runtime](shell/applet-runtime.md) defines manifest, host, policy, and
  compiled-implementation gates and records which built-ins are genuinely live.
- [Testing harness](development/testing-harness.md) defines isolated nested
  sessions, virtual outputs, visual baselines, and the required display matrix.
- [Implementation roadmap](development/implementation-roadmap.md) distinguishes
  completed foundation/compositor work from the upcoming desktop milestones.
- [Profile schema v1](reference/profile-schema-v1.md) and
  [theme schema v1](reference/theme-schema-v1.md) document the data currently
  accepted by the loaders.
- [Compositor control protocol 1.0](reference/compositor-control-v1.md)
  documents the experimental D-Bus methods, signals, and transaction boundary.
- [Notification presentation protocol 1](reference/notification-presentation-v1.md)
  documents the authenticated resident-host-to-shell snapshot boundary.
- [Coding practices](development/coding-practices.md) keeps the implementation
  modular and legible to future agents.
- [Documentation maintenance](contributing/documentation-policy.md) states when
  wiki pages and architecture decision records must change.
- [Architecture decisions](adr/index.md) records durable choices and their
  consequences.

## Product constraints

- The native session is Wayland; XWayland is started only for legacy clients.
- Qt 6 is the UI and application foundation. Plasma is not the shell runtime,
  though focused KDE Frameworks may be reused behind explicit boundaries.
- Conventional floating, minimizing, maximizing, and snapping remain first
  class alongside grouped tabs and splits.
- Appearance and workflow are independently configurable through themes and
  layout profiles.
- The shell, compositor, and default resident services target no more than
  500 MiB aggregate idle PSS and less than 1% average idle CPU on the reference
  machine.
- Accessibility and keyboard equivalents are required for every pointer-only
  customization or window-management operation.

The Foundation, Compositor MVP, and Hybrid interaction milestones are complete.
The repository now has a qualified virtual/nested compositor substrate and an
integrated process-local Hybrid runtime with scene-resident paint-only chrome,
native member detach, complete page operations, collapsed task/switcher
identity, member/transient policy, semantic keyboard and accessibility paths,
bounded unload recovery, live context-menu policy, and process-local evidence.
Final qualification results and limitations are recorded in the
[testing harness](development/testing-harness.md).
Physical DRM/GPU/input qualification remains a release gate.
Pages distinguish accepted contracts from planned implementation; unresolved
durable decisions belong in an ADR rather than being silently embedded in
code.
