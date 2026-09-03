# ADR-0056: Isolate clipboard capture in a volatile resident host

- **Status:** Accepted
- **Date:** 2026-09-02
- **Owners:** Clipboard platform service
- **Supersedes:** None
- **Superseded by:** None

## Context

[ADR-0031](0031-volatile-bounded-clipboard-history.md) establishes a pure,
volatile model, an allowlist for stored MIME types, and purge-on-authority-loss
generation fencing. C1 must observe compositor selections and serve metadata to
shell consumers without moving payload ownership into QML, persisting content,
or trusting an unauthenticated lock signal. A toolkit clipboard API would hide
the data-control offer and peer identity needed for those boundaries.

The staging `ext-data-control-v1` protocol supplies regular and primary
selection observation. Adding an implicit `wlr-data-control` fallback would
create a second generated protocol and compatibility surface without an
accepted requirement or evidence that it preserves identical behavior.

## Decision

Run one activatable `qindaqt-clipboard-host` per user session. It owns the C0
model, complete payloads, one direct client of a pinned
`ext-data-control-v1.xml`, and the private `org.qindaqt.Clipboard1` object.
Keep the protocol, exact-owner asynchronous client, Wayland adapter, and host
as separate modules with public injected seams.

Authenticate the Wayland peer with `SO_PEERCRED` and provide only that PID to
the fail-closed session-lock monitor. Capture requires a confirmed Settings1
Boolean `true` sourced specifically from `user-overrides`, conclusively unlocked
authenticated state, and a live data-control device. Schema and profile defaults
are not consent. Any authority loss stops capture and purges the model. Do not
fall back to `wlr-data-control`; report the adapter unavailable.

Expose only canonical bounded QCDL descriptor bytes over Clipboard1. Fence all
mutations by exact owner, process epoch, model generation, snapshot revision,
and caller-scoped request id. Remember bounded results to make exact duplicate
delivery idempotent, reject conflicting reuse, and never automatically replay
an uncertain request. Copying retrieves payload only inside the host and
publishes it through the adapter.

Never persist, synchronize, export, or log clipboard payloads. Install D-Bus
activation and a hardened systemd user unit, neither of which gains filesystem
state authority.

## Consequences

- UI processes receive bounded metadata and intents, not a payload store.
- Lock, Settings1, compositor, or bus uncertainty produces empty or unavailable
  truth instead of retaining readable history.
- Pinned XML makes protocol source changes reviewable and fake-server tests can
  exercise real generated client code without a host compositor.
- Compositors lacking `ext-data-control-v1` cannot provide QindaQt history;
  ordinary toolkit clipboard behavior is unaffected.
- The resident process is intentionally session-volatile and restart changes
  epoch, invalidating all earlier client authority.
- Both shipped Settings schemas default clipboard history off. The host also
  rejects a true schema/profile default, preserving explicit consent if a
  future default regresses.

## Revisit when

Revisit the lack of a `wlr-data-control` fallback only after a separate bounded
compatibility contract and contained interoperability evidence exist. Revisit
the process boundary only if a future security design provides equivalent
payload isolation, exact-owner fencing, and purge guarantees without granting
shell presentation code clipboard authority.
