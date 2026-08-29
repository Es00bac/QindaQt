# Manager assignment: Fable display/output architecture analysis

- **Timestamp:** 2026-08-27T12:07:00-06:00
- **From:** Manager
- **To:** Elara Finch, display/output architecture analyst
- **State:** Scheduled for 2026-08-27T12:44:00-06:00, after the reported Claude
  subscription reset; this record does not claim a live process.
- **Exact product base:**
  `dc29c88911f0ed6d381211027f16f46bbf92a07c`
- **Product worktree:**
  `/home/cabewse/work_SPaC3/container-wm-workers/fable-display-architecture`
- **Product path ownership:** none; analysis and planning only

## Outcome

Produce an implementation-ready display/output architecture for multi-monitor,
mixed/fractional DPI, rotation, mirroring, hotplug, connector replacement,
lid/suspend, safe apply/confirm/revert, shell panel/work-area reconciliation,
and Hybrid grouped-window migration and restoration.

The assignment must preserve the existing compositor/platform/shell/settings
boundaries or explicitly propose a superseding ADR. It must split deterministic
unit/private-D-Bus/nested KWin evidence from physical DRM/KMS, GPU, lid, and
suspend qualification, and decompose implementation into collision-free
vertical slices for other workers. The analyst does not implement any slice.

## Provider verification and handoff rule

The manager requested Claude Fable 5 at maximum effort with no fallback model.
The run has no file-editing tools. Its raw initialization event, terminal result,
and unchanged product worktree must be verified before any handoff is accepted
or attributed to Fable. The manager will then publish the exact board-ready
analysis in a new append-only record and route its cross-lane questions to the
platform, compositor/Hybrid, shell/customization, Settings1, native-app/design,
and release owners.
