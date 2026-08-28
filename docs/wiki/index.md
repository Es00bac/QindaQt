# QindaQt project wiki

> **QindaQt is pronounced “kinda cute”** (`KYNE-duh kyoot`,
> /ˈkaɪndə kjuːt/). Use the `QindaQt` spelling in written project names.

QindaQt is a modular, Wayland-first Qt desktop environment. Its distinguishing
feature is a hybrid window model: ordinary windows remain independently usable,
but users may combine them into a movable container containing tabs and
recursive splits.

This wiki is the canonical source for architecture, behavior, and contributor
workflow. It changes with the code and is validated as part of the repository.

## Reading the wiki offline

Open `build/docs/qindaqt-project-wiki.epub` in Calibre's E-book viewer rather
than opening this Markdown file directly. Calibre treats one directly opened
`.md` file as the complete book and therefore does not include linked sibling
pages. Generate the complete EPUB with `tools/build-wiki-epub`; its internal
links include every page in this wiki. The regular relative Markdown links
below remain the source-of-truth links for repository browsers and MkDocs.

## Start here

- [Architecture overview](architecture/overview.md) describes the runtime and
  its process boundaries.
- [Module boundaries](architecture/module-boundaries.md) defines ownership and
  dependency direction.
- [QST-1 semantic design tokens](architecture/design-tokens.md) defines the
  immutable theme/accessibility derivation and read-only QML boundary.
- [QindaQt.Controls 1.0](shell/controls.md) defines the compiled token-only
  primitives, accessible state behavior, preview integrity, and focused gates
  used by first-party interfaces.
- [Launcher](shell/launcher.md) records the bounded installed-application
  model, deterministic categories/search/ranking, launch intents without
  execution, and the pinned/recent and presentation boundaries.
- [Compositor and session integration](architecture/compositor-session.md)
  records the exact KWin ABI, launcher/plugin boundary, completed Compositor
  MVP evidence, and explicit later-milestone boundaries.
- [Notification service](architecture/notifications-service.md) records the
  bounded model, freedesktop adapter, authenticated host/client transport,
  descriptor provisioning, and remaining service policy.
- [Audio service](architecture/audio-service.md) records the typed Audio1
  model/client/service boundary, confined WirePlumber adapter, activation, and
  isolated-runtime qualification.
- [Power and brightness](architecture/power-service.md) records the accepted
  Power1, shell-action, fail-closed backlight, and session-bound activation
  architecture; PB-0 pure candidates exist while every resident/platform slice
  remains pending.
- [Pure brightness model](architecture/brightness-model.md) fixes the PB-0
  stable-ID fixture, mirror collapse, raw-range math, owner-loss behavior, and
  transport-free composition boundary.
- [Clipboard service](architecture/clipboard-service.md) records the volatile
  bounded history model, canonical media policy, privacy/generation fencing,
  metadata search, and codec seam; the live Wayland adapter remains a later
  milestone.
- [Display service](architecture/display-service.md) records the pure Display1
  values, identity/topology boundaries, and deterministic transaction model;
  its runtime service and compositor adapter are later milestones.
- [Notification presentation](shell/notification-presentation.md) records the
  bounded production popup/center behavior, Settings1-fed interruption policy,
  authenticated lock-state privacy gate, and unqualified boundaries.
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
- [Compositor control protocol 1.1](reference/compositor-control-v1.md)
  documents the experimental D-Bus methods, signals, and transaction boundary.
- [Notification presentation protocol 1](reference/notification-presentation-v1.md)
  documents the authenticated resident-host-to-shell snapshot boundary.
- [Settings1 protocol 1](reference/settings1-v1.md) documents the generic,
  bounded user-settings snapshot and optimistic-commit boundary.
- [Audio1 protocol version 1](reference/audio1-v1.md) documents the fixed
  device/stream snapshot, handle lineage, operation results, and bounds.
- [Display1 version 1](reference/display1-v1.md) documents display value bounds,
  identity/registry rules, topology projection, codecs, and transaction states.
- [Power1 version 1](reference/power1-v1.md) documents bounded Power values,
  privacy-preserving handles, canonical codecs, and fail-closed validation.
- [QindaQt Text Editor](apps/text-editor.md) documents the first-party local
  UTF-8 document, atomic-save, external-change, menu, theme, and accessibility
  boundaries.
- [QindaQt.AppShell 1.0](apps/application-shell.md) documents the narrow shared
  lifecycle, action/menu, injected integration, portal, focus, accessibility,
  and installed-consumer boundary for first-party QML applications.
- [QindaQt File Manager](apps/file-manager.md) documents the first-party local
  directory navigation, breadcrumb/history, bounded file-launch, QST-1/Controls
  presentation, and accessibility boundaries.
- [Settings Appearance route](apps/appearance-settings.md) documents the
  first-party appearance settings surface: validated drafts, QST previews,
  per-key Settings1 commits, and recovery truth.
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
- The shell, compositor, and default resident services initially target no more
  than 1,024 MiB aggregate idle PSS and less than 1% average idle CPU on the
  reference machine. Functional nested-session qualification takes precedence;
  the ceiling is a measured starting budget to refine after the complete desktop
  boots, renders, and accepts isolated test input reliably.
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
