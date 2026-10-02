# ADR-0341: Unify protected capture and input sessions

- **Status:** Proposed — manager-approved implementation boundary; source/native qualification pending
- **Date:** 2026-10-02
- **Owners:** Platform, Compositor, Program Manager
- **Amends:** [ADR-0324](0324-native-portal-capture-boundaries.md), [ADR-0335](0335-native-remote-input-portal.md)

## Context

The frontend sends combined RemoteDesktop session selections to its selected
ScreenCast backend. Splitting resident input and protected capture loses session
ownership. Resident ProcessCapture has ordinary displays rather than the
inherited compositor capture capability; it cannot qualify protected sharing.

## Decision

Compose the existing RemoteDesktop, InputCapture and private Clipboard adaptors
with ScreenCast in the existing fixed, compositor-launched protected broker.
All share its RequestRegistry; the ScreenCastSourceDelegate joins the existing
private session owners and borrows AuthorityCapture. No new registry, pixel
transport, fabricated frontend sender, grant or capability is introduced.

The broker separately exports `org.qindaqt.PortalCapture1` at
`/org/qindaqt/PortalCapture1`, implementing the existing `org.qindaqt.Portal1`
interface and AttachSessionWithDisplay method. It reuses the unchanged same-UID
unique first-caller, selected-compositor/public peer and FD-lifetime checks.
The supervisor watches and attaches this endpoint without launching/activating
the broker. Resident Portal1 stays independent. ProcessAccessConsent obtains
only ordinary display FDs and exact native Unlocked truth; CompositorEis uses
that selected owner. Ordinary attachment never grants capture.

The fixed broker retains nondumpability, no standalone activation, inherited
QCC1/native receipt, control-peer checks, capture-only capability scopes and
fixed executable path. It gains input-family composition, not resident Settings,
notifications, audio or unrelated services. Capture helper monitor consent,
frozen selection and actual PipeWire publication remain unchanged. Native lock,
attachment owner loss and uncertainty retire all requests/transports/producers
and exit the broker. Input consent executable is a trusted configured path; only
the noninstalled test broker substitutes its exact actual-input helper.

## Consequences and acceptance

Route Screenshot, ScreenCast, RemoteDesktop, InputCapture and Clipboard together
to the protected broker; the compositor EIS allowlist must admit its exact owner.
Root owns this coordinated route/allowlist/package change. Combined sessions
remain nonpersistent. Standalone ScreenCast asks explicit remember and returns
frontend-owned restore tokens only after actual protected monitor consent.

Private-bus tests must preserve independent first-owner attachment lifetimes.
Actual frontend gates must show EIS/input/clipboard and protected frames together,
Close/lock/caller/owner retirement, remembered selection/restore, and native
identities. Source and old input PASS are not final-release qualification.
