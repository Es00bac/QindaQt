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
- QST-1 immutable semantic-token derivation, total caller-owned accessibility
  transforms across loader-valid schema-v1 colors, exact five-theme WCAG pair
  gates, clean installed-C++ consumption, and a read-only GUI-thread QML
  singleton;
- a Qt Quick shell preview showing panels, applets, and the shared-title-bar
  window-container concept at arbitrary preview dimensions;
- an exact KWin 6.6.5 source/ABI pin, the `qindaqt-wm` launcher, a
  release-matched plugin, and an atomic versioned container-control bridge;
- deterministic live KWin integration covering three mapped Wayland windows,
  rootless XWayland, output/input inventories, page activation, detach,
  singleton unwrapping, release, and exact frame restoration;
- dynamic plugin-unload recovery for four live clients grouped in two
  containers;
- the Hybrid interaction value layers: revisioned multi-container topology,
  recursive constraint solving, full independent-window restore state,
  consuming pointer/keyboard gesture semantics, shared-chrome render/hit plans,
  and a loadable QindaQt KDecoration3 plugin;
- the production Hybrid collaborator graph inside KWin: atomic full-state scene
  transactions, rollback-safe group reflow, semantic gesture translation,
  dock preview, member-anchored scene-image chrome with compositor-side ordinary
  input routing, atomic group-context adoption and its outer-title menu, native
  member detach/focus policy, transient following, group placement/actions,
  scene-restart synchronization, unload release, and read-only runtime
  diagnostics;
- Shell/customization foundations: atomic profile-panel expansion,
  collision-free logical geometry and work areas; exclusive, revisioned
  panel/applet editing with manifest-aware preflight, preview, rollback, and
  undo/redo; and pure window-aware visibility/reservation policy;
- a production `qindaqt-shell` process that turns solved panels into real
  LayerShellQt surfaces, with fail-closed replacement and live nested-KWin
  work-area proof at 1080p, WUXGA, and 1440p;
- production applet resolution through validated manifests, placement and host
  policy, a compiled implementation registry, and least-authority grants, plus
  live built-in locale-aware clock and capability-empty notification-center
  entries;
- a bounded, revisioned notification model, freedesktop Notifications 1.3
  D-Bus adapter, installable resident ownership/expiry host, authenticated
  private presentation server/client, descriptor-only token handoff, and
  essential host/shell session supervision, plus bounded production popups and
  an active/recent center with logical-DPI clamping, serialized operations,
  authoritative failure recovery, bounded busy/error presentation, a dedicated
  entry in every stock profile, and a shell-owned `Meta+N` action registered
  through KF6 GlobalAccel without applet notification authority, plus injected
  Settings1-persisted Do Not Disturb with immediate low/normal popup suppression,
  an explicit critical bypass, Active/Recent retention, and no replay on
  disable, plus a compositor-PID/unique-owner-authenticated KScreenLocker
  monitor that clears and denies every notification projection unless the
  session is conclusively unlocked;
- isolated development-session planning and declarative single-, multi-output,
  mixed-DPI, rotation, and hotplug scenarios, with honest reporting of the
  subset the current virtual backend actually applies;
- an integrated contained virtual-desktop S0+S1 harness boundary with an
  authenticated private bubblewrap stage, exact production package and
  topology contracts, bounded readiness/resource accounting, durable failure
  evidence, and identity-safe teardown; its private 1080p boot row remains
  unqualified pending the exact Settings application-identity integration; and
- strict compiler warnings, unit tests, source-shape checks, and this wiki.

The repository now boots a real compositor and has completed its Compositor
MVP qualification. Hybrid interaction has process-local live pointer grouping,
native-decoration detach, keyboard parity, complete page/tree operations,
readable public state, runtime decoration proof, member focus/transient policy,
close/ungroup policy, lifecycle synchronization, and grouped plugin-unload
restoration. Final qualification passed every gate recorded in the
[testing harness](testing-harness.md), so Hybrid interaction is complete. This
is not yet a daily-use desktop session: panel windows, the clock, and the
bounded notification presentation/entry path are implemented, and the complete
installed Notification Live shortcut, focus, Do Not Disturb, service/shell
replacement, authenticated private-lock, scale, and teardown matrix is
qualified. Alternative lockers and multi-seat support remain unqualified.
Audio has a bounded service/runtime slice, while power, Bluetooth, menu, task,
launcher, tray, and clipboard entries remain unavailable or visual fixtures
rather than complete live integrations.

## Milestones

| Milestone | Outcome | State |
| --- | --- | --- |
| Foundation | Domain invariants, schemas, preview, scenario harness, documentation policy | Complete |
| Compositor MVP | Tracked KWin base, nested Wayland session, XWayland, output/input adapters, atomic container protocol | Complete |
| Hybrid interaction | Pointer and keyboard docking, paint-only shared outer decoration, native member drag, split/page reorganization, focus/transient policy, restore | Complete |
| Shell and customization | Real panels/docks, window-aware hiding/layers, global menu, direct drag-from-settings editing, notifications | In progress (production panels and the complete installed Notification Live shortcut/focus/DND/restart/private-lock matrix are qualified; reveal UI, global menu, remaining applets, direct WYSIWYG editing, and whole-shell qualification remain) |
| Platform services | Audio, power, brightness, Bluetooth, network, clipboard, display/color/font settings, portals and policy | In progress (Audio1 typed stack and isolated null-device proof plus Display D0 inventory, D1 protocol/identity/topology/transaction foundation, and D2 resident service/exact-owner adapter implemented; PB-0 bounded Power1 values/codecs, deterministic aggregation, and pure brightness composition/math integrated at WIRED; Display client/writer/UI, resident Power service/client/upstream adapters, hardware qualification, and other providers pending) |
| First-party experience | Settings center and core applications with accessibility and consistent theming | In progress (QST-1 and reusable QindaQt.Controls are independently qualified; Text Editor S1, bounded local File Manager S0, and the narrow installed QindaQt.AppShell 1.0 action/lifecycle/portal/focus/accessibility boundary are executable; full Settings Center routes, later File Manager capabilities, Terminal, app migrations, cross-app and nested-session matrices, and live accessibility bridge pending) |
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

## Completed Hybrid interaction milestone

The implemented process-local runtime translates edge/tab gestures into atomic
session topology commands; moves individual members and complete page trees;
detaches leaf or split pages; normalizes, activates, and resizes splits;
constraint-solves every page; preserves complete independent state and outside
focus; moves/resizes/maximizes complete groups; follows transients; handles
member focus actions; propagates group output/workspace/activity/layer context;
reconciles one member-anchored scene image per group; and releases groups before
plugin teardown. Qinda macOS is the production style,
with left traffic lights that reveal `x`, `_`, and `[]` glyphs on cluster hover
and stable logical tabs laid out visually right to left.

Closed acceptance items include:

- a nested exact `Meta+Shift+Left` path that creates one process-local split,
  reads its real nonzero revision/schema-1 snapshot, survives a complete
  compositor scene reinitialization with the same visible anchored group,
  rejects click-through from an ordinary covering window and a popup-dismiss
  press, preserves a focused normal-type transient outside topology, then
  detaches through a plain native member-title drag; the member keeps its size
  and follows the pointer while its sibling restores its exact baseline and all
  owners clear;
- page-identity operations for cross-container move, leaf/split-page detach,
  one-member extraction, and whole-page regrouping with intentional tab-to-edge
  rejection;
- autoloading keyboard modes for dock/detach, complete-group move,
  active-divider adjustment, and complete-group resize, all with
  commit/cancel and pass-through semantics;
- a nonblocking Close All/Ungroup/Cancel policy;
- a live outer-title context menu plus queued atomic whole-group output,
  workspace, activity, Keep Above, and Keep Below adoption with rollback;
- member maximize/fullscreen focus mode with minimize/close/native-drag and
  shutdown restoration, plus dialog/transient following and lifecycle
  focus/stack/output synchronization;
- live mapped `QindaDecoration` class proof; and
- dynamic unload while a Hybrid-owned group exists, followed by service and
  authority removal, exact KWin restoration, and continued client usability.

The milestone's explicit focused and live selectors remain authoritative rather
than an unfiltered shared-registry count. Final qualification passed clean Debug
and Release registries, the focused Hybrid suites, a fresh bridge-only build,
the applied virtual-display subset, lifecycle/security and live menu/unload
proofs, focused ASan+UBSan, ten consecutive runs of both Hybrid live workflows,
strict documentation/source checks, and final audit. Exact counts, commands,
and limitations are maintained in the [testing harness](testing-harness.md).
The milestone is **Complete**.

Live heterogeneous mixed-DPI migration, physical input/DRM/GPU,
suspend/resume, hotplug/rotation/lid policy, and performance/memory evidence are
later Platform or Release gates. Persisted topology across login remains later
desktop work; it is not silently claimed by this interaction slice.

The exact command paths, rollback boundaries, and honest coverage split are in
[Hybrid topology](../architecture/hybrid-topology.md),
[Hybrid constraints](../architecture/hybrid-constraints.md),
[Hybrid chrome](../architecture/hybrid-chrome.md), and the
[testing harness](testing-harness.md).
