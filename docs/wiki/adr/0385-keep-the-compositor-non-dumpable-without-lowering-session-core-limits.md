# ADR-0385: Keep the compositor non-dumpable without lowering session core limits

- **Status:** Accepted — fork source candidate; installed delivery pending
- **Date:** 2026-10-16
- **Owners:** Compositor fork, native lock
- **Amends:** [ADR-0299](0299-native-locker-private-launch-and-authentication.md)
- **Superseded by:** None

## Context

ADR-0299 requires the compositor, greeter and PAM worker to disable core dumps
and dumpability so that no ordinary session client can read the compositor's
private locker transport. The fork implemented this in the compositor by
setting `RLIMIT_CORE` to `{0, 0}` and `PR_SET_DUMPABLE` to 0. Resource limits
are inherited across `fork` and `execve`, and an unprivileged process can never
raise its hard limit. The compositor starts Xwayland and `qindaqt-session`,
which starts the shell and every application, so the hard limit of 0 reached
the whole session: no crash anywhere in it left a core (stability findings item
16, 2026-10-16; the owner's session showed `0 0` for the compositor, session and
shell while sddm passed `unlimited`).

## Decision

The compositor protects itself with `PR_SET_DUMPABLE(0)` only and verifies it.
A process whose dumpable flag is 0 gets no core file from the kernel, and none
is piped to `systemd-coredump`; ptrace attachment needs `CAP_SYS_PTRACE` under
the Yama mode the launcher already requires. The compositor does not change
`RLIMIT_CORE`, so its children inherit the limits the display manager set.
`execve` resets dumpability, so the protection never leaks to children.

The locker, PAM worker, capture broker and capture helper keep disabling their
own cores and dumpability at their start, as before; Yama protects each one in
the exec-to-main gap. Nothing else in ADR-0299 changes.

## Consequences

- Crashes of the session, shell, Xwayland and applications leave cores again
  wherever the inherited limit allows.
- The compositor itself still never dumps core.
- The fork's helper test `qindaqt.lock-core-limit` runs the production
  protection in a child process and requires the child to be non-dumpable and
  its exec'd grandchild to inherit unchanged core limits; the native lock
  matrix row `launcherConnectionIsolation` checks the same in the compositor.

## Revisit when

A future protection needs a property that only a lowered resource limit can
provide, or the kernel changes the meaning of `PR_SET_DUMPABLE`.
