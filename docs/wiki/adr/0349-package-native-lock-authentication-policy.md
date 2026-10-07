# ADR-0349: Package the native lock authentication policy

- **Status:** Accepted
- **Date:** 2026-10-07
- **Owners:** Native lock authentication, Gentoo packaging
- **Supersedes:** None
- **Builds on:** [ADR-0299](0299-native-locker-private-launch-and-authentication.md)

## Context

The native worker fixes its PAM service name to `qindaqt-lock`. Both installed
Gentoo hosts have the r13 worker and greeter but lack that named service in
the PAM configuration roots. Gentoo's `other` policy requires `pam_deny` for
authentication and account checks, so the worker reports denial without a
conversation. The greeter deliberately shows a response field only for an
actual PAM prompt; the resulting locked screen has no password field.

The earlier contained production proof checked compositor launch, standard
lock role, buffer commit and frame feedback with PAM masked. Those observations
did not establish authentication policy or prompt readiness. The independent
private worker regression must distinguish missing-service denial from a
configured synthetic password conversation without using owner credentials.

## Decision

Gentoo owns the named service in a small `sys-auth/qindaqt-lock-pam` package,
selected by the QindaQt profile and exact delivery. Future desktop recipe
revisions require this package at runtime. Historical recipes and archives
remain immutable. This allows the installed worker to obtain its missing
configuration without a compositor or desktop restart; each authentication
attempt loads the current PAM policy. Other distributions supply the same
fixed service with their own normal authentication/account policy.

The Gentoo service uses mandatory `system-auth` substacks for authentication
and account checks, retaining the site's normal password and failed-attempt
policy. Password changes are denied. Native keyring hooks are optional and
never grant unlock. The worker's notification-only session hook contains
`pam_permit` plus optional native keyring notification; it imports no
`system-login`, `pam_systemd`, seat or logind session setup. This is an
authentication check inside the existing desktop, not another login.

The policy is root-owned, mode 0644 and managed through Portage's normal
CONFIG_PROTECT. Delivery verifies recipe, package image, mandatory-signed
binary identity, installed bytes and ownership. A successful launch-only
greeter gate cannot close the authentication/prompt boundary. Private tests
use disposable PAM configuration and synthetic modules; actual password
authentication and physical presentation remain distinct observations.

## Consequences

- A missing named PAM service becomes an explicit packaging regression rather
  than a prompt/UI workaround or a password-policy bypass.
- The existing locker gains the correct policy at its next attempt without
  terminating open applications.
- Distribution policy stays outside the greeter, native protocol and worker
  wire libraries; those process and authority boundaries do not change.
- A future change must preserve mandatory authentication/account checks and
  the absence of a second login session.

See [Native session locking](../architecture/native-session-lock.md) and
[Testing harness](../development/testing-harness.md).
