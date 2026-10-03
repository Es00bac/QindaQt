# ADR-0345: Hold physical native power cutover until its receipts pass

- Status: Accepted
- Date: 2026-10-03
- Owners: QindaQt program manager and native power integration
- Scope: Installed desktop power mode and physical login gate

## Context

The reviewed native-exclusive assembly in [ADR-0338](0338-own-scoped-display-power-and-shared-idle-composition.md)
requires authenticated lock, display, sleep, settings, source and Power1 idle
consumer receipts before the session may claim exclusive authority. The r5
desktop package selected that mode on qinda-top. Two fresh physical SDDM logins
on October 3 started KWin and then returned to SDDM when the native power
readiness barrier reported unavailable prerequisites/receipts and the session
exited 2. Power1 itself answered `GetSnapshot`; a resident bus owner alone did
not satisfy the complete assembly.

A temporary `--native-power off --no-powerdevil` session child kept the same
compositor and desktop running. The fallback established login viability, not
full native idle, display-off or suspend policy parity. Plasma PowerDevil is
absent from the de-Plasma laptop, so an optional PowerDevil child must not be
counted as an installed fallback.

## Decision

The r6 installed desktop profile compiles
`QINDAQT_NATIVE_POWER_EXCLUSIVE=OFF` until the physical receipt barrier can
pass. The source implementation and its fail-closed exclusive startup barrier
remain intact. Power1 stays installed and separately monitored. Future profile
cutover to ON requires a fresh physical login and genuine compositor inventory,
lock, sleep, settings, source and idle-consumer receipts; service activation or
private-fixture tests alone do not qualify it.

This supersedes only the package cutover timing in ADR-0338, not its authority
model or verified source behavior. The temporary per-user launch override may
be removed after the signed r6 package is installed and its normal login path
is qualified.

## Consequences

Normal login no longer exits because this unqualified exclusive assembly
fails. Native idle dim, display-off and suspend parity remain an open delivery
gate while the profile is OFF. The exact failed receipt must be identified and
repaired before re-enabling exclusive mode. The installed package, not a
per-user wrapper, is the durable source of the normal login default.

See [Idle policy](../architecture/idle-policy.md) and
[Power service](../architecture/power-service.md).
