# ADR-0026: Contain integrated virtual desktop qualification

- **Status:** Accepted
- **Date:** 2026-08-28
- **Owners:** Session and display qualification
- **Supersedes:** None
- **Superseded by:** None

## Context

[ADR-0015](0015-qualify-function-before-resource-refinement.md) requires the
production compositor, session, shell, resident services, and applications to
boot as one reviewable virtual desktop before resource tuning becomes the
primary objective. Existing nested tests isolate XDG paths and D-Bus, but their
timeout boundary does not make every descendant's exit a kernel fact. The
developer account can also access host input nodes, and a copied
`WAYLAND_DISPLAY`, session-bus address, or runtime directory would let a
qualification defect reach the active desktop.

Settings1 and Audio1 packages install D-Bus activation descriptors whose
`Exec` values are expanded at configure time. Those values cannot select a
relocated test prefix reliably. Whole-desktop qualification must prove the
staged binaries, not whichever executable an ambient descriptor or `PATH`
selects.

## Decision

The first integrated boot row requires bubblewrap PID, network, IPC, UTS, and
user namespaces with parent-death and new-session behavior. It constructs an
empty root, private `/dev`, `/tmp`, `/run`, HOME, XDG roots, machine identity,
session bus, and Wayland socket. It mounts only discovered read-only system
tool roots, the exact read-only QindaQt stage/source/probe inputs, and bounded
writable runtime/log/evidence directories. It never binds host input/uinput,
the host runtime directory, display/session sockets, home/configuration, or a
render node.

Because the root is empty, the harness constructs only the conventional
merged-usr `/lib` and `/lib64` aliases as relative links to the already mounted
`/usr/lib`. This exposes no additional host path and lets an explicitly
discovered system or Homebrew executable reach its authenticated ELF
interpreter without assuming the host root's symlink layout.

The empty root also receives fresh caller-owned passwd and group files that
name only the mapped qualification UID/GID and its private `/home/qindaqt`.
Their exact paths, ownership, regular-file shape, and contents are authenticated
with the run root before read-only binding; the host account databases are
never exposed.

The outer harness builds argv and environment from typed values rather than
inherited environment state. It requires both an explicit manager allocation
token and a nonblocking per-user cross-worktree advisory lock before starting
the sandbox. CTest serialization is an additional local guard, not the
cross-worktree authority.

Production executables and the compositor plugin are resolved as regular,
non-symlink files beneath one staged prefix. Settings1 and Audio1 are started
through those exact paths. Their service descriptors are checked for package
identity but their `Exec` values are never used for runtime selection. Host
tools are discovered by CMake and passed explicitly; the harness has no
hard-coded `/usr/bin` executable dependency.

Every observed role binds a PID, executable, kernel start time, process group,
and expected parent role before cleanup. Teardown revalidates executable plus
start time before bounded TERM-to-KILL signaling; PID reuse is never signaled.
The preserved cleanup ledger repeats the authenticated role/PID/group/path/
start-time identity and records only the terminal observation phase:
`already-exited`, `term`, or `kill`. These phases do not claim graceful exit.
The PID namespace and `--die-with-parent` remain the final fail-closed boundary
on assertion, timeout, or driver failure. A run root can be deleted only when
its exact run ID, build parent, and sentinel all match.

S1 embeds one immutable `1920x1080@1` topology: KWin compositor, production
session/shell/notification graph, Settings1, Audio1, Settings and Text Editor
applications, the public-D-Bus probe, one `Virtual-1` output, matching nonzero
output/visibility generations, one combined development input device, and at
least one mapped and committed production `dock` layer surface on that output.
Every consumed dock record must carry a canonical positive decimal-string
client PID equal to the externally authenticated current production-shell PID;
a foreign or stale replacement surface cannot satisfy the row. An unavailable
surface-evidence method is a dependency failure; ordinary window inventory
cannot be substituted.

Service ownership alone is not readiness. One bounded loop reacquires the
complete public-D-Bus snapshot until output/generation, input, dock, and both
application records are simultaneously valid. A method or malformed service
reply fails immediately; an absent startup service or incomplete application/
dock state may retry only until the hard deadline. Application evidence retains
the compositor-observed `applicationId`, window ID, and title. It never creates
an expected identity from a title match.

Every readiness probe reserves a fixed one-second lifetime before it starts;
the driver does not shorten that lifetime to the outer deadline's final
milliseconds and does not start a probe unless the full lifetime remains. The
15-second outer cap is therefore never extended. Each consumed stdout snapshot
is flushed into that attempt's archived process log before schema or topology
validation, and an outer/probe deadline reports the exact last pending
observation rather than a raw subprocess timeout.

Before the private run root is removed, every attempt archives all regular
artifacts, all process logs, combined sandbox output, and exact run/result/
timeout metadata beneath a fresh build-local
`desktop-session-results/<run-id>` directory. That persistent directory has its
own build/run-bound sentinel and may not preexist or be a symlink. Failure and
timeout paths use the same archive-finally boundary as success.

## Consequences

- The safe unit and staged-package rows can run without a private-runtime
  allocation. The boot row cannot.
- Bubblewrap and unprivileged user namespaces are required for this boot gate.
  There is no implicit process-group fallback.
- The first row deliberately exposes no DRM render node and uses QPainter. It
  proves boot/topology, not screenshots, synthetic interaction, GPU behavior,
  or physical devices.
- Settings1/Audio1 activation packaging remains independently testable, while
  whole-desktop identity binds their owners to direct staged executables.
- The current-base merge extends Notification Live's development-only
  compositor allowlist from its two exact notification roles to include only
  the production `dock` scope consumed here. Notification Live keeps selecting
  its own two roles; the harness does not copy or modify its private shell
  implementation. This is the narrow additive successor to ADR-0020's original
  evidence allowlist.
- Evidence and logs are copied out before the authenticated run root is
  removed, including on timeout and cleanup failure. A successful row requires
  the exact 1,048,576 KiB PSS schema/ceiling, authenticated terminal-phase
  records for every topology role, and zero surviving recorded PIDs.

## Revisit when

Revisit the boundary if bubblewrap cannot run in a required CI environment, a
kernel-backed equivalent provides the same PID/device/socket containment, the
desktop gains a dedicated installed qualification supervisor, or render-node
capture becomes part of the functional boot row. Any fallback must remain an
explicit configured mode with separately labelled evidence.
