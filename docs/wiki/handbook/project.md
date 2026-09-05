# Project and philosophy

QindaQt is a free and open-source, modular, Wayland-first desktop environment
built around Qt 6. It combines ordinary independent application windows with
containers that can hold tabs and recursive splits. A person can group related
work, move it together, and separate it again while the applications remain
ordinary clients. The project is more than a window-layout library: it includes
a compositor integration, desktop shell, service boundaries, reusable UI
components, first-party applications, and a qualification harness.

## Design commitments

| Commitment | Practical consequence |
| --- | --- |
| Preserve familiar window operations | Floating, minimizing, maximizing, snapping, and independent windows coexist with grouping. |
| Make workflow a choice | Profile data controls panel arrangement and workflow; changing a theme does not require changing the layout. |
| Keep authority in the right process | The compositor controls windows; services control platform state; QML presents confirmed state and requests actions. |
| Build small, explicit modules | Policy, persistence, platform adapters, and presentation have separate owners and visible dependencies. |
| Make customization accessible | Pointer gestures need keyboard equivalents, stable focus behavior, and accessible feedback. |
| Prefer bounded work | Inventories, payloads, history, operations, and recovery have explicit limits rather than unbounded background behavior. |
| Fail honestly | Unknown, stale, denied, and pending states must remain visible; a button click is not proof that a device changed. |
| Prove complete behavior | An integrated executable path and reviewed evidence matter more than source volume, activity, or screenshots of a preview. |

Qt is the application and UI foundation. KWin supplies the compositor substrate;
Plasma is not the QindaQt shell runtime. Focused KDE facilities are reused behind
explicit interfaces. The exact binary plugin pin is maintained rather than
assuming compatibility with a different KWin release. Wayland is native and
XWayland is available for legacy applications.

## Resource and release philosophy

The initial combined idle target for the shell, compositor, and default resident
services is at most 1,024 MiB PSS and below 1% average idle CPU on the reference
machine. This is a budget to measure, not a claim about an arbitrary machine.
Reliable contained boot, rendering, interaction, and cleanup precede further
resource optimization. Physical DRM, GPU, and input qualification remains a
release boundary beyond virtual-session evidence.

At this snapshot, Foundation, Compositor MVP, and Hybrid interaction are marked
qualified. Shell and first-party work have executable slices; platform maturity
varies by step. Release qualification is absent in the outcome ledger. The
latest handoff records a verified installation boundary, which is narrower than
claiming all release work or hardware coverage complete.

## Licensing and governance

`LICENSE.md` assigns GPL-3.0-or-later to standalone applications and the shell,
LGPL-3.0-or-later to reusable libraries and SDK interfaces, and preserves
compatible upstream licensing and attribution. Source files carry SPDX IDs.
Dependencies retain their own licenses.

Architecture is governed by [module boundaries](../architecture/module-boundaries.md)
and [ADRs](../adr/index.md). Durable cross-cutting changes need a new decision
record; superseded choices stay readable as history. See [quality and
contribution](quality.md) for the delivery and documentation rules, and the
[handbook index](index.md) for the other reading paths.
