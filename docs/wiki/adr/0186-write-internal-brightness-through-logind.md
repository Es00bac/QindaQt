# ADR-0186: Write internal brightness through logind

- **Status:** Accepted
- **Date:** 2026-09-17
- **Owners:** Power platform service and the Power Settings route
- **Supersedes:** the sysfs-only write rule of
  [ADR-0148](0148-admit-internal-panel-brightness-through-power1.md) and the
  corresponding sentence of
  [ADR-0060](0060-confine-production-power-upstreams.md)
- **Superseded by:** None

## Context

[ADR-0148](0148-admit-internal-panel-brightness-through-power1.md) admitted
internal-panel brightness into Power1 and wrote it by opening
`/sys/class/backlight/<device>/brightness` directly. It also recorded the
consequence that a panel whose attribute is not writable stays visible and
read-only, and called that "the supported refusal state, not a reason for a
helper".

On real hardware that refusal state is the *only* state. On `qinda-top` (Lenovo
IdeaPad, AMD 5825U) the sole backlight device is `amdgpu_bl0`, type `raw`, and
its `brightness` attribute is `0644 root:root`. `SysfsBacklightSource::rescan()`
admits a device only when that file `isWritable()`, so the panel was published
`Unavailable` / `LogindError` / `backlight-read-only`: the Settings brightness
slider and the brightness keys were both dead, with no supported way to make
them work. The installed unit additionally sets `ProtectKernelTunables=true`,
which mounts `/sys` read-only inside the service, so even a permission change
on the host would not have been enough.

The refusal was also not the whole truth. logind already performs exactly this
write for the session that owns the seat, and it does so without a polkit
prompt. Verified on the laptop as the ordinary user on 2026-09-17:

    busctl call org.freedesktop.login1 \
      /org/freedesktop/login1/session/_3120 \
      org.freedesktop.login1.Session SetBrightness ssu \
      backlight amdgpu_bl0 <value>

succeeds. The same call against `.../session/auto` from an ssh shell fails with
"no seat", because `auto` resolves to the *caller's* session, and an ssh
session has no seat.

The alternatives were all worse: a setuid helper, a shipped udev rule that
loosens permissions on a kernel attribute for every local user, or a polkit
action of our own. Each adds privilege where the platform already grants it.

## Decision

Internal-panel brightness is written through the user's own seat session on the
system bus, and sysfs remains the observation source.

- A new abstract seam, `BacklightWriter`
  (`src/services/power_service/include/qindaqt/services/power_service/adapters/backlight_writer.h`),
  takes a sysfs device *name* and a raw kernel value. It owns the
  `BacklightWriteStatus` / `BacklightWriteOutcome` vocabulary that previously
  lived on the sysfs source. The seam is transport-free by contract: the
  transport fence in `tests/services/power_service/check_boundary.cmake`
  forbids `sysfs_backlight_source.*` from naming a bus, a daemon or a session,
  which is the reason the seam exists rather than a direct call.
- `LogindBacklightWriter` implements it with
  `org.freedesktop.login1.Session.SetBrightness(ssu)`, subsystem `backlight`.
  It resolves the session once and caches it: `.../session/auto` is accepted
  only when that session actually reports a non-empty `Seat`, and otherwise the
  writer picks this uid's seat session from `Manager.ListSessions` (`a(susso)`,
  so the seat arrives in the same reply), preferring the active one. A failed
  probe is re-tried at most every two seconds, because the sysfs source asks
  `available()` on every rescan and rescan follows every write.
- `SysfsBacklightSource` takes an optional writer and changes exactly two
  rules. Admission: a device is `Ok` when the kernel attribute is writable by
  this process **or** the injected writer reports available. Write: a device
  whose attribute this process cannot write is delegated to the writer, and a
  successful delegated write is followed by the same `actual_brightness`
  re-read as a direct write, so the observation still comes from the kernel and
  never from the request. A writable attribute keeps the direct path and never
  reaches the writer.
- Production composes the two (`composeUpstream`); the `unavailable` upstream
  mode composes no writer at all; tests inject their own.
- Two stable diagnostics cross the seam. `logind-unavailable` means no seat
  session could be resolved, and is published as the device diagnostic so the
  Settings route can say so. `logind-refused` means logind answered and said
  no. A refusal keeps the resolved session; only a stale-object, unknown-service
  or no-reply error re-resolves, once, and retries.

Nothing in this decision escalates privileges. There is no setuid helper, no
udev rule, no polkit action, and no QindaQt-owned privileged process: the write
is delegated to a service that already holds the authority for the seat the
user is sitting at.

## Consequences

- The built-in panel is adjustable in Settings and by the brightness keys on
  ordinary laptop hardware, where the kernel attribute is root-owned. This is
  what ADR-0148 called an unsupported state and is now the normal one.
- ADR-0148's consequence "a denied write completes `Failed` with
  `backlight-read-only`; nothing escalates privileges or retries" is narrowed:
  it still holds when no writer is composed, and otherwise a denied *direct*
  write is delegated before any failure is reported. The no-escalation half of
  that sentence is unchanged.
- `ProtectKernelTunables=true` in the installed unit is no longer a blocker.
  `/sys` stays readable for observation, and the write leaves the unit over
  `AF_UNIX`, which `RestrictAddressFamilies` already allows.
- The write blocks on bounded system-bus round trips. Each individual call is
  capped at 2 s, and because one *resolution* can issue the `auto` seat probe,
  `ListSessions`, and one `Active` probe per candidate, resolution as a whole is
  capped at 3 s of wall clock: a logind that accepts connections and then stalls
  every reply cannot turn one request into dozens of sequential timeouts. When
  the budget runs out with a seated candidate already in hand, that candidate is
  used rather than reporting unavailable — an inactive seat session of this uid
  is the right panel far more often than no panel at all. Callers already
  serialize internal-brightness requests one at a time
  (`ProductionBatteryCollaborator`), so a stall delays requests rather than
  losing or reordering them. Making the seam asynchronous is a separate change
  to the whole `BacklightWriter` contract, not a local edit.
- A session with no seat — an ssh shell, a system-scope unit — still cannot
  change the brightness of a seat it does not own, and now says so with
  `logind-unavailable` instead of blaming file permissions.
- The ADR-0148 target rule, its ordering, readback and no-replay fences, and the
  Power1 protocol version are all unchanged. No new dependency: libsystemd is
  not used, and QtDBus was already linked by the adapter layer.

## Revisit when

- A privilege-separated brightness API replaces both paths.
- The seam must become asynchronous because a real logind stall is observed.
- KWin's backlight provider (PB-5) supplies connector topology and changes
  which device is selected.
