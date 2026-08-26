# Implementation roadmap

QindaQt is being built as vertical, testable slices. This page separates code
that exists from accepted contracts and longer-term product scope; it is
updated whenever a milestone changes state.

## Current foundation

The repository currently builds and tests:

- a Qt Core container domain model with recursive splits, pages, activation,
  detachment normalization, validation, and schema-versioned JSON persistence;
- validated profile schema v1 with ten built-in workflow families;
- validated theme schema v1 with light, dusk, dark, high-contrast, and Qinda
  macOS themes;
- a Qt Quick shell preview showing panels, applets, and the shared-title-bar
  window-container concept at arbitrary preview dimensions;
- isolated development-session planning and declarative single-, multi-output,
  mixed-DPI, rotation, and hotplug scenarios; and
- strict compiler warnings, unit tests, source-shape checks, and this wiki.

The preview is not yet a compositor or daily-use desktop session. Static
applet chips are visual fixtures, not live audio, power, Bluetooth, menu, or
clipboard integrations.

## Milestones

| Milestone | Outcome | State |
| --- | --- | --- |
| Foundation | Domain invariants, schemas, preview, scenario harness, documentation policy | Complete |
| Compositor MVP | Tracked KWin base, nested Wayland session, XWayland, output/input adapters, atomic container protocol | Planned |
| Hybrid interaction | Pointer and keyboard docking, shared outer decoration, preserved member drag regions, split/tab reorganization, restore | Planned |
| Shell and customization | Real panels/docks, window-aware hiding/layers, global menu, direct drag-from-settings editing, notifications | Planned |
| Platform services | Audio, power, brightness, Bluetooth, network, clipboard, display/color/font settings, portals and policy | Planned |
| First-party experience | Settings center and core applications with accessibility and consistent theming | Planned |
| Release qualification | Hardware matrix, performance/memory gates, migrations, packaging, recovery and upgrade paths | Planned |

Each milestone lands behind stable module boundaries rather than accumulating
inside one shell process. A feature is complete only with its failure behavior,
keyboard/accessibility path, persistence where applicable, focused tests,
nested display coverage, and updated owning wiki page.

## Immediate compositor slice

The next slice creates the reproducible downstream KWin workspace and boots
`qindaqt-wm` inside the existing nested-session harness. It will expose only
the smallest versioned surface needed to map compositor toplevels into the
tested container model. The first end-to-end acceptance case groups two test
clients into one movable outer container, creates a split, drags either
preserved member title region back out, and restores the original independent
windows without losing geometry or focus.

See [ADR-0001](../adr/0001-use-kwin-as-compositor-base.md) for the compositor
choice and [Window containers](../architecture/window-containers.md) for the
behavioral invariants.
