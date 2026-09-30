# ADR-0316: Authenticate Power1 idle state with a targeted receipt

**Status:** Accepted

**Supersedes:** the idle-state query authority transport portion of
[ADR-0306](0306-power1-idle-inhibitors-and-stage-policy.md). ADR-0306 remains
the policy decision for scope ownership, atomic admission, and stage behavior.

## Context

Qt 6.11.1 exposes an empty `QDBusMessage::service()` for method replies, and
the installed D-Bus session policy permits eavesdropping on unrequested
replies. A reply from a method call addressed to the expected unique owner
therefore cannot authenticate the reply's actual sender. The legacy
`GetIdleInhibitorCapabilities` and `GetActiveIdleInhibitorScopes` methods remain
useful for compatibility and diagnostics, but their Qt method replies cannot
authorize lock or power policy.

## Decision

Power1 adds `RequestIdleInhibitorStateWithReceipt(s nonce) -> void` and the
targeted `IdleInhibitorStateReceipt(s nonce, u supported, u active)` signal.
The producer takes the actual unique caller from `QDBusContext`, snapshots the
current service epoch and scope state, and sends the signal only to that
caller. The void method reply means transport progress only.

The Qt client creates a fresh 32-character lowercase hexadecimal nonce,
subscribes to the exact current unique owner before calling, and accepts state
only from an actual signal message whose sender, path, interface, member,
signature, nonce, request, and owner generation all match. One request is
pending for at most 1500 ms. Owner replacement, stop, send/query error, or
timeout fails closed. Stale, duplicate, and unmatched receipts do not publish
state. Existing getters and `IdleInhibitorsChanged(uu)` remain wire-compatible;
the latter is likewise published only after checking its actual signal sender.

The ordinary unique-owner and session attachment checks still prove process
identity and lifetime only. This receipt protocol adds request correlation and
actual signal sender evidence; it does not claim executable attestation or
resist a compromised bus daemon.

## Consequences

No scope becomes supported because of this transport change. Capabilities stay
zero until every requested scope has a live consumer. Consumers must treat a
missing current receipt as unknown and fail closed. Power policy and the
manual-lock exemption in ADR-0306 do not change.
