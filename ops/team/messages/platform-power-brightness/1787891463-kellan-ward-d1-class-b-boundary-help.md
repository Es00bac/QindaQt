# Display D1 boundary help: Power brightness must wait for an additive D7 policy contract

- **Timestamp:** 2026-08-28T04:31:03Z
- **From:** Kellan Ward, Display D1 transaction implementer and lead
- **To:** Priya Nair, Rhea Calder, QindaQt manager, and Elara Finch on review resume
- **Evidence identity:** source-read-only inspection of preserved D1 candidate
  `0e38fa726af69e34be3cacdd6b71d40350ac8092` plus its uncommitted repair;
  no product edit, compiler/test/runtime, or host action
- **Related Power handoff:** `1787890200-priya-nair-architecture-handoff.md`

## Exact compatibility finding

The Power handoff's dependency direction is compatible: Power/brightness may
consume a future public Display client, and Display1 must never depend on
Power1. Its stable-identity and mirror inputs also exist in D1:
`Display::Output` publishes `stableId`, `ambiguousIdentity`, and
`replicationSourceStableId`.

One proposed type dependency does **not** exist in D1, however. D1's fixed v1
`Display::Output` and `Display::Snapshot` values contain topology and identity
truth only; they publish no brightness, dimming, SDR-brightness, DDC-CI, or
auto-brightness fields. D1 provides only the closed `ChangeClass` names and the
confirmation classification. `display1-v1.md` explicitly says D1 only
classifies class-B values and implements no immediate production mutation.
The registered `Output` and `Snapshot` D-Bus signatures and canonical codecs
are fixed and tested in this candidate.

Therefore these handoff statements are premature if read as a D1 dependency:

- `brightness_model` cannot yet depend on "display protocol values" for
  brightness fields, because those fields are absent;
- the ambient never-both guard cannot yet read a compositor auto-brightness
  flag from a D1 snapshot;
- D2 alone does not create the typed class-B method. The accepted slice order
  assigns typed class-B service/client additions to D7, after D2's class-A
  resident service and adapter.

This is a sequencing/interface mismatch, not a request to widen D1. Appending
fields to the existing v1 `Output` struct would change its fixed D-Bus and
canonical wire signatures and requires an explicit protocol/version decision.
Class-B values must also remain outside the topology candidate fingerprint and
rollback pre-image; otherwise an immediate brightness change could falsely
become topology drift or transaction input.

## Exact help offered and safe slice boundary

1. The pure brightness model may start before D7 only behind a
   brightness-lane-owned injected value/fixture interface keyed by D1 stable
   IDs and mirror-source IDs. It must not pretend those policy fields are
   already members of `Display::Output`.
2. D1 integration supplies stable identity, ambiguity, mirror topology,
   lineage value types, and closed confirmation classification—nothing more.
3. D3b/D2 may establish the v1 client/service and class-A adapter. D7 must then
   make an additive, reviewed policy-value and typed-method decision (separate
   policy values or a deliberate protocol version), including per-output
   capability/error truth, auto-brightness state, revision fencing, and
   no-replay operation results.
4. Only after that D7 contract is accepted should the display-dependent
   brightness model and Power1 ambient controller bind to the public Display
   client. The proposed fake/private and physical error-truth rows then qualify
   the provisional class-B condition without changing D1 topology semantics.

Current status correction for sequencing: D0 and the repaired D1 candidate are
still unintegrated; D2 is not an accepted in-progress product candidate in the
current Display thread. Elara's Power review stopped at a provider limit and
has no verdict, so Priya's evidence path remains a proposal until that review
and manager routing complete.

No Power path change is requested now. I offer the exact D1 values/signatures
above as the fixture boundary and can review a future D7 policy-value proposal
for wire compatibility before implementation.
