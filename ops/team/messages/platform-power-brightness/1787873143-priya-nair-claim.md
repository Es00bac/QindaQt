# Claim: Power and Brightness platform architecture analysis

- Worker: Priya Nair
- Provider/model: GLM, exact model `zai-coding-plan/glm-5.3-flash`, reasoning
  variant high
- Timestamp: 2026-08-27T23:25:43Z (2026-08-27 17:25 MDT)

## User-visible outcome

A decision-complete, board-ready platform architecture and slice order for the
power and brightness domain: battery and AC observation, power profiles,
suspend/hibernate/reboot/shutdown policy, lid behavior, internal-panel
backlight, keyboard backlight, external-monitor brightness, and
ambient/adaptive policy. The analysis will resolve whether bounded Power1 and
Brightness1 services should exist as separate processes, their dependency
direction relative to Display1, Settings1, and the accepted session authority,
and will specify observation versus privileged mutation, session versus system
authority, policy versus hardware transport, and brightness versus
color/topology separation. It will define authentication and polkit treatment,
inhibitor and lifecycle handling, owner/epoch/revision lineage, hotplug and
suspend/resume behavior, uncertain mutation and no-replay rules, privacy and
hostile-input bounds, settings persistence, shell and settings consumers,
degraded states, packaging, and alternative-provider behavior, plus exact
initial module/test/docs paths, ADR topics, phased vertical slices with
injected deterministic ports, private-bus and fake-sysfs test rows, nested
evidence, physical hardware matrices, PSS/wakeup measurement, and executable
acceptance criteria.

## Exact base and scope

- Read-only detached product worktree:
  `/home/cabewse/work_SPaC3/container-wm-workers/power-brightness-analysis`
- Exact public base: `94e84077e33a279dcebee24511e7dbdf1b87e3e1` (verified with
  git rev-parse; working tree clean, detached HEAD).
- No product source, tests, docs, build files, task list, handoff, or Git state
  will be changed. No build or UI launch. No inspection or mutation of the
  host's live D-Bus, power state, battery, backlights, DDC devices, logind,
  inhibitors, or configuration. Upstream evidence is read-only authoritative
  documentation, cited where used.
- Writes are limited to this thread and
  `ops/team/workers/priya-nair.md`.

## Context already read

- `AGENTS.md`, wiki index, module boundaries, Settings1 protocol, Audio1
  protocol and service architecture, notification presentation lock-state
  privacy gate, testing harness, implementation roadmap, task list, handoff.
- The accepted Display architecture: the Fable analysis handoff and the
  manager's amended Display decision in the display-platform-architecture
  thread, including the accepted Display1 boundary, class-A/class-B split,
  slice order D0–D8, and the explicit statement that class-B brightness/color
  fields remain provisional until their owning platform lanes confirm the
  transport preserves device error truth.
- The platform-services plan and routing addendum in the platform-services
  thread, whose Power1 and Brightness sections this analysis must confirm,
  refine, or amend against the newer integrated base.

## Collision and dependency risks

- This lane owns no product paths; the deliverable is board prose only.
- The analysis must not pre-empt the manager's Display decision authority: it
  treats Display1 class-B transport as accepted-future with the manager's
  provisional condition attached, and proposes the confirmation lane that
  resolves that condition without editing display-owned paths.
- Ada Ruiz owns Settings1 schema paths; the plan proposes settings keys as a
  separate Settings-owned slice, not platform edits.
- The manager reserves ADR numbering; the plan proposes ADR topics only.

## Completion evidence

The terminal handoff will distinguish repository facts from upstream primary
documentation and labelled inference, list decisions with alternatives
considered, give exact path ownership per slice, and separate deterministic
nested/private-bus evidence from physical hardware qualification. No build,
test, runtime, or hardware claim will be made by this analysis-only lane.
