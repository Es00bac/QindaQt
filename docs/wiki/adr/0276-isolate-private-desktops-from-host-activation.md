# ADR-0276: Isolate private desktops from host activation

- **Status:** Accepted
- **Date:** 2026-09-26
- **Owners:** Platform (session)
- **Supersedes:** None (narrows ADR-0082 and ADR-0094 to physical desktops)
- **Superseded by:** None

## Context

Virtual desktop captures ran with a private D-Bus broker but shared the user's
runtime directory. Session startup reached the real systemd user manager's
native control socket, replaced its desktop environment, and restarted its
portal services. This repeatedly invalidated the live Gabbee dictation shortcut.
A nested desktop can also inherit the host broker, so blocking only the native
manager route is insufficient.

## Decision

The session application determines activation scope from its existing direct
parent lifetime witness. Only a `kwin_wayland` executable with explicit `--drm`
backend evidence before its child payload is physical. Nested backend flags,
conflicting evidence, unknown parents, and unreadable evidence are private.

Private scope is the default for activation and resident-refresh helpers. It
skips broker activation publication and all shared-manager mutations. An explicit
fake-manager endpoint remains available to hermetic tests; production exposes
no environment or CLI override. Private harnesses own their broker setup.

Physical desktop scope preserves the existing native sd-bus fallback even on
a bootstrapped private production bus. Broker address shape cannot establish
physical desktop ownership; ADR-0170's legitimate private-bus login remains
supported.

## Consequences

Launching a virtual or windowed desktop cannot redirect the live desktop's
activation environment or restart its resident services. Focused gates verify
backend classification, no shared-broker publication, no shared-manager calls,
physical native routing, and fake-manager override compatibility.

Private tests relying on supervisor publication must seed their own broker
before activating services. This is a lifecycle guard against accidental
cross-session interference, not a security boundary against same-user code.

See [compositor and session integration](../architecture/compositor-session.md#private-desktop-isolation)
and the [testing harness](../development/testing-harness.md#private-session-activation-setup).
