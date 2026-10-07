# Implementation roadmap

QindaQt is being built as vertical, testable slices. This page separates code
that exists from accepted contracts and longer-term product scope; it is
updated whenever a milestone changes state.

The October 3 deployment checkpoint has the signed native desktop and
compositor installed on both hosts, actual legacy credentials sealed and
authenticated by complete exact retries, and all retired Plasma packages
removed through normal scoped Portage cleanup. Fresh physical login/PAM
adoption and physical hardware journeys remain open; this does not advance
their qualification state. See `docs/HANDOFF.md`.

## Current implementation and qualification boundary

The [October 7 everyday audit](everyday-desktop-audit.md) is the current
cross-module usability assessment. Its [delivery plan](everyday-desktop-plan.md)
adds proposed work without advancing milestone or feature-ledger progress.
The module owner pages and installed handoff take precedence over historical
foundation checklists when deciding what still needs implementation.

The current native tree includes:

- window containers and ordinary window behavior, a native compositor fork,
  shell panels, launcher, tasks, notifications, tray, clipboard and menus;
- resident Settings, Audio, Network, Bluetooth, Display, Power and clipboard
  boundaries with graphical Settings consumers;
- native lock authentication, keyring, credential prompts, power policy and
  portal implementations, with explicit per-family tests and installed limits;
- File Manager local/network operations, separate Removable Media, Text Editor
  crash recovery/printing, Viewer, Welcome and shared application controls;
- separately packaged QQ_Term, QindaTK and Office applications, including
  existing semantic agent interfaces in the owning sibling repositories; and
- private nested compositor, package, unit, accessibility-metadata and
  lifecycle evidence described in the [testing harness](testing-harness.md).

This inventory is not complete installed acceptance. The October 7 incidents
leave physical unlock and fresh-login keyring proof open; accepted Power1 and
keyring reattachment source still need delivery. Physical sleep, radios,
docking, assistive technology and third-party application journeys retain
their own qualification gates. File Manager device integration and several
cross-filesystem operations, basic PDF tools, advanced network setup and a
joined agent-context experience have specific remaining work. Consult the
owning pages and audit before creating a task; do not rebuild existing modules
because an older milestone summary still calls them future work.

## Milestones

The September 19 first-party slice adds the [QindaTK image/PDF
Viewer](../apps/viewer.md) and [common application
defaults](../apps/default-applications.md), including QindaMPV for media.
This is a source delivery with focused rendering, presentation and association
gates; installation and physical desktop qualification are separate evidence
recorded in the integration handoff. It does not complete the wider
First-party experience milestone.

| Milestone | Outcome | State |
| --- | --- | --- |
| Foundation | Domain invariants, schemas, preview, scenario harness and documentation policy | Complete at its recorded boundary |
| Compositor MVP | Tracked compositor base, nested Wayland/XWayland and atomic container protocol | Complete at its recorded boundary; physical matrix remains a release gate |
| Hybrid interaction | Grouping, pages/splits, ordinary windows, focus, restore and recovery | Complete at its recorded boundary; whole-session physical acceptance is separate |
| Shell and customization | Panels/docks, menus, launcher, tasks, notifications, tray, clipboard and editing | Implemented slices with remaining whole-shell accessibility, hardware and newcomer-workflow qualification; see owning pages and everyday audit |
| Platform services | Audio, network, Bluetooth, display, power, keyring, locking, portals and policy | Native installed checkpoint exists; current incident delivery and physical/end-to-end acceptance remain open |
| First-party experience | Consistent graphical applications, settings and terminal integration | Current applications substantially exceed the original S0 slices; file/device, PDF, administration and agent-coverage gaps are planned in the everyday audit |
| Release qualification | Hardware, application compatibility, migrations, packaging, recovery and upgrade paths | In progress; installed Gentoo checkpoint does not establish wider beta readiness |


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
