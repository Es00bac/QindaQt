# Implementation roadmap

QindaQt is being built as vertical, testable slices. This page separates code
that exists from accepted contracts and longer-term product scope; it is
updated whenever a milestone changes state.

## Current buildable state

The repository currently builds and tests:

- a Qt Core container domain model with recursive splits, pages, activation,
  detachment normalization, validation, and schema-versioned JSON persistence;
- validated profile schema v1 with ten built-in workflow families;
- validated theme schema v1 with light, dusk, dark, high-contrast, and Qinda
  macOS themes;
- a Qt Quick shell preview showing panels, applets, and the shared-title-bar
  window-container concept at arbitrary preview dimensions;
- an exact KWin 6.6.5 source/ABI pin, the `qindaqt-wm` launcher, a
  release-matched plugin, and an atomic versioned container-control bridge;
- deterministic live KWin integration covering three mapped Wayland windows,
  rootless XWayland, output/input inventories, page activation, detach,
  singleton unwrapping, release, and exact frame restoration;
- dynamic plugin-unload recovery for four live clients grouped in two
  containers;
- isolated development-session planning and declarative single-, multi-output,
  mixed-DPI, rotation, and hotplug scenarios, with honest reporting of the
  subset the current virtual backend actually applies; and
- strict compiler warnings, unit tests, source-shape checks, and this wiki.

The repository now boots a real compositor and has completed its Compositor
MVP qualification, but it is not yet a daily-use desktop session. Static
applet chips are visual fixtures, not live audio, power, Bluetooth, menu, or
clipboard integrations, and the user-facing hybrid interaction is the next
milestone.

## Milestones

| Milestone | Outcome | State |
| --- | --- | --- |
| Foundation | Domain invariants, schemas, preview, scenario harness, documentation policy | Complete |
| Compositor MVP | Tracked KWin base, nested Wayland session, XWayland, output/input adapters, atomic container protocol | Complete |
| Hybrid interaction | Pointer and keyboard docking, shared outer decoration, preserved member drag regions, split/tab reorganization, restore | Planned |
| Shell and customization | Real panels/docks, window-aware hiding/layers, global menu, direct drag-from-settings editing, notifications | Planned |
| Platform services | Audio, power, brightness, Bluetooth, network, clipboard, display/color/font settings, portals and policy | Planned |
| First-party experience | Settings center and core applications with accessibility and consistent theming | Planned |
| Release qualification | Hardware matrix, performance/memory gates, migrations, packaging, recovery and upgrade paths | Planned |

Each milestone lands behind stable module boundaries rather than accumulating
inside one shell process. A feature is complete only with its failure behavior,
keyboard/accessibility path, persistence where applicable, focused tests,
nested display coverage, and updated owning wiki page.

## Completed compositor milestone

The reproducible KWin workspace, launcher, release-matched plugin, and
`org.qindaqt.Compositor1` transaction path form the qualified Compositor MVP.
The complete 40-test suite passes in both Debug and Release configurations. Its
live workflow takes three real Wayland clients through docking revision 1,
page creation revision 2, page activation and reactivation revisions 3–4,
third-member detach/restore revision 5, and automatic singleton
unwrap/restore revision 6 before redocking and explicit release. A separate
workflow groups four live clients into two containers, dynamically unloads the
plugin through KWin, and independently verifies exact restored frames,
minimized state, and continued client usability. Both live workflows have
passed ten consecutive stress repetitions.

This milestone proves the virtual compositor substrate, a Weston 15 headless
parent-Wayland path, rootless XWayland, read-only production control policy,
atomic model/scene publication, output/input inventory, staged-failure cleanup,
and lifecycle restoration. It deliberately does not claim the finished user
interaction or physical hardware qualification.

The shared outer decoration, preserved member drag regions, consuming
pointer/keyboard docking, constraints, and richer window-state restoration
belong to **Hybrid interaction**. Applying heterogeneous output topology,
rotation, hotplug, and lid policy belongs to **Platform services**. Physical
DRM/KMS and GPU/input-device coverage remains a **Release qualification** gate;
the DRM launcher command path alone is not hardware evidence.

See [ADR-0001](../adr/0001-use-kwin-as-compositor-base.md) for the compositor
choice and [Window containers](../architecture/window-containers.md) for the
behavioral invariants. Exact current evidence and limitations are in
[Compositor and session integration](../architecture/compositor-session.md).
