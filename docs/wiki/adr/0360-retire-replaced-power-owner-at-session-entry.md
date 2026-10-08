# ADR-0360: Retire a replaced Power1 owner at session entry

- **Status:** Accepted
- **Date:** 2026-10-08
- **Owners:** Platform session supervision and Power service
- **Supersedes:** None (extends ADR-0094's replaced-owner retirement list)
- **Superseded by:** None

## Context

The manager's actual laptop observation found a Power1 process whose executable
was deleted by a package replacement and whose running digest differed from the
installed program after a fresh login. Network and Bluetooth matched their
installed binaries in that observation. Process presence alone did not prove
Power1 was current. The old process must not retain package-versioned state
indefinitely across login.

ADR-0025 records a broader session-bound activation/arbitration contract. This
repair does not implement or relax that arbitration. The actual current
supervisor starts its shell child before registering Session1; the shell's
public Power client can request activation then. Native lock runtime also
starts a public Power client after its Session1-backed composition succeeds.
Neither order constitutes a new winner-only foreign-binding guarantee.

## Decision

Add only org.qindaqt.Power1 to the closed reviewed replaced-activation names.
Do not add its unit to unconditional resident refresh. A healthy owner,
missing owner, unknown identity or Private session is untouched.

Resolve the constructing bus's current unique owner and its daemon-reported
PID. Require the same user, a nonzero process starttime and raw executable
proof. For Power, the raw target must be exactly the compiled installed
qindaqt-power-service path followed by the kernel deleted marker. A different
deleted executable is insufficient.

Hold a pidfd and compare identity observations across acquisition to reject PID
reuse. Immediately before signalling, re-resolve the same unique owner/PID and
re-read the same user/starttime/executable. Signal only the held lifetime with
pidfd_send_signal; unsupported pidfds or any failed query grant no signal.
Settings and portal keep their existing inclusion semantics with the same
lifetime guard. No bare numeric-PID signal fallback exists.

After a successful signal, wait at most two seconds for the old unique owner to
disappear or change. This is evidence of name retirement, not a promise that a
new resident is ready. All query waits are bounded; timeout/signal failure
returns best effort and startup continues. Private scope never signals, even
when a test proc root is supplied.

Ordinary public Power client activation starts the installed binary through
the existing packaged D-Bus/Systemd descriptor after retirement. This helper
does not call StartServiceByName or RestartUnit, publish new activation
environment, alter brightness/profiles/preferences, or take over a healthy
foreign binding.

## Consequences

- A proven replaced Power executable can retire before consumers start.
- Healthy Power owners retain their process and state.
- An unresolved retirement remains visible and does not block desktop login.
- The unit's existing private-bus Type=dbus limitation remains ADR-0170's
  boundary; retirement does not make activation universally reliable.
- A pidfd pins process lifetime, not an atomic D-Bus-owner-plus-exec transaction.
  The immediate owner and identity rechecks fail closed on observed changes;
  no stronger indivisible ownership guarantee is claimed.
- Focused private-broker tests cover exact matching replaced and healthy child
  lifetimes, changed owner/PID/lifetime, other-user/unknown identity, missing
  bus/name, signal failure, Private suppression and bounded unresolved return.
  They do not retire a live installed Power process or qualify hardware.

See [compositor/session](../architecture/compositor-session.md) and
[Power service](../architecture/power-service.md). Historical
[ADR-0025](0025-arbitrate-session-bound-power1-activation.md) remains intact.

## Source and focused native acceptance

Exact c7e2e8a61492d201725663f1b086b7b631889d0b has independent manager source and raw-evidence acceptance: strict six owning targets built, six CTests and56 Qt checks passed with zero failure, skip or blacklist. All11 declared raw-evidence digests were independently checked; the actual child PIDFD retirement fixture runs only on its own private broker/lifetime. Manager integration gates remain required before delivery. This does not qualify installed owner retirement or a fresh ordinary desktop startup.
