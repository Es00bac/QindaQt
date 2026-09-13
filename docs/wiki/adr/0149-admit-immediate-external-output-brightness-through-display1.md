# ADR-0149: Admit immediate external-output brightness through Display1

- **Status:** Accepted
- **Date:** 2026-09-12
- **Owners:** Display platform services
- **Supersedes:** None
- **Superseded by:** None

## Context

[ADR-0016](0016-display1-transaction-authority.md) made Display1 the only
QindaQt production writer to KDE output-management. D1 classifies brightness as
class B, which bypasses confirmation, but it left every class-B change
provisional until its platform lane proves device error semantics. The
[Power architecture](../architecture/power-service.md) reserves this as PB-4
("Display D7 class-B brightness policy/method") and lists external-monitor
brightness as having no authority. Users need a real external-display
brightness control. PB-5 will present that control as a Settings slider.

The installed KWin 6.6.6 source fixes the forces:

- Output-device version 9 advertises `capability_brightness`. The `brightness`
  event exists from version 8, and `set_brightness` needs management version 9.
  Both use a 0–10000 scale. KWin's DRM backend grants the capability for HDR,
  for a brightness device (backlight or DDC/CI), or when SDR software
  brightness is allowed.
- KWin stores `set_brightness` for any live device and acknowledges the
  configuration even when the backend ignores the value. Its virtual backend
  has no brightness control and ignores the value. A configuration fails with
  "One of the relevant outputs is no longer available" only when an output is
  added or removed after the configuration was created.
- A private nested `kwin_wayland --virtual` run on this KWin confirms the
  acknowledgement behavior. It acknowledges `set_brightness` on non-capable,
  out-of-scale, and disabled targets without publishing any brightness change.
  It also keeps a virtual output's runtime UUID across a compositor restart.
- D0 `Compositor1.Outputs` carries no brightness and does not advance its
  generation when brightness changes. The D1 revision is that generation. A
  same-fingerprint newer revision during `Observing` would read as an apply
  rejection.

## Decision

Display1 protocol version 1 gains two additive methods. `GetSnapshot`, its
signature, the canonical codecs, and the D1 machine are unchanged. `Changed`
remains the single invalidation hint for both reads.

- `GetBrightness() -> (ustta(sbbu))` returns a `BrightnessSnapshot`: protocol
  version, epoch, topology revision, brightness revision, and one
  `(stable ID, capable, observed, value)` row per output.
- `SetOutputBrightness((stsu)) -> (uuusttss)` takes an epoch, a brightness
  revision, a stable ID, and a value. It returns an `OperationResult` of kind
  `ImmediatePolicy`.

**Observation.** D4's own output-management connection is the observation
authority. At each device `done`, registry, or transport edge it publishes the
complete, unambiguous, ready device set for its owner generation. Each device
carries its connector, runtime UUID, enabled state, capability, observation,
and value. Capability requires management version 9, a device bound at version
9 or later, and the advertised bit. Observation requires a device bound at
version 8 or later and a value within the scale. Anything less publishes
generation zero with no devices.

The resident joins each accepted D1 snapshot output to exactly one device whose
connector and runtime UUID both match exactly once. Any other output publishes
a cleared row. The publication is a sibling of the snapshot:

- `topologyRevision` names the exact joined snapshot revision;
- rows follow that snapshot's order;
- `revision` is monotonic within the epoch and advances whenever a row or the
  joined topology revision changes;
- losing the snapshot withdraws the publication.

**Request.** The exact-owner client pins the bus owner. The request pins the
epoch, a brightness revision, and a stable ID. It is stale unless the epoch
matches and the revision lies between the revision at which the current
topology was joined and the current revision; a value-only change since then is
last-writer-wins. Admission requires all of:

- a `Ready` D1 machine and `Safe` safety;
- a known, unambiguous, external, enabled, non-replica output;
- a joined device that is enabled, capable, and observed.

An equal observed value succeeds as a no-op without a write. One request may be
pending; another returns `Busy`.

**Class-A coexistence.** While a brightness request is pending, `Preview`
rejects `TransactionActive`. D4 also refuses a topology apply during a
brightness write, and a brightness write during a topology apply. No apply or
revert can overlap an immediate write; a staged candidate waits.

**Writer.** D4 sends exactly one configuration containing `set_brightness` and
then `apply`, and only for the exact ready, enabled, capable connector whose
UUID matches. It refuses everything else locally, before any protocol request.

**Completion.** An accepted call replies only when it finishes, as a delayed
D-Bus reply.

- `Succeeded` requires both the compositor's `applied` acknowledgement and a
  republished observation of the requested value. Its observed revision is
  that publication, and `Changed` is emitted before the reply.
- A protocol `failed` finishes `Rejected` with `CompositorRejected`.
- These finish `Uncertain` with a typed error, and nothing is replayed:
  transport uncertainty, a writer owner-generation change, D0 owner or
  transport loss, service stop, any topology revision change, or the D1
  observation deadline.

## Consequences

- External brightness becomes a real control wherever KWin advertises
  brightness control. Other outputs publish honest `capable=false`, and Power's
  authority map now names Display1 for KWin-managed external brightness.
- An acknowledgement is not proof. A write KWin acknowledges but ignores
  finishes `Uncertain(Timeout)`, not `Succeeded`.
- Internal panels stay with Power1 under
  [ADR-0148](0148-admit-internal-panel-brightness-through-power1.md), and
  Display1 refuses them. DDC/CI enablement policy stays reserved for PB-6;
  Display1 only consumes KWin's resulting capability.
- Clients join `GetBrightness` to `GetSnapshot` by `topologyRevision`, and may
  see `Changed` repeat a topology revision. The Qt Display client does not bind
  these methods yet; that binding and the Settings slider are PB-5.
- Evidence rows:
  - `qindaqt.display-protocol-brightness`
  - `qindaqt.display-service-brightness`
  - `qindaqt.display-service-resident-brightness-private-bus`
  - `qindaqt.display-writer-brightness-port`
  - `qindaqt.display-writer-wayland-brightness`
  - the private nested KWin proof listed in
    [Display writer](../architecture/display-writer.md)

## Revisit when

- KWin starts failing `set_brightness` for incapable outputs, or reports
  per-request errors.
- D0 inventory publishes brightness inside its own generation.
- A Display1 version 2 folds class-B values into `GetSnapshot`.
