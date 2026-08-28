# Pure brightness model

The PB-0 `brightness_model` candidate composes authoritative display and
keyboard brightness values without transport, mutation, persistence, or
presentation authority. It consumes Power1 values plus a brightness-lane-owned
fixture. It does not include Display1 headers or reinterpret Display1 output
structures. Focused build/tests pass; until independent exact-commit review
accepts the preserved boundary, this page remains a normative candidate
contract rather than integrated evidence.

## Independent fixture boundary

A fixture generation carries an owner-present flag, bounded opaque service
epoch, positive revision, and at most 32 display values. Each display value has
only:

- a bounded opaque stable ID;
- an optional `replicationSourceStableId`;
- the ambiguous-identity marker; and
- an optional epoch-scoped opaque Power1 internal-backlight handle.

Connector names, EDID, topology mutation fields, brightness policy, and
Display1 implementation types are not representable. A root alone may carry a
backlight mapping. Stable IDs and mappings are unique; replica sources must
exist; partial handles, self-reference, and cycles fail atomically. A mapping
whose handle epoch differs from the current Power snapshot is unavailable with
typed lineage-mismatch truth and can never bind by opaque ID alone. Owner loss
is canonical only when epoch, revision, and fixture rows are cleared, which
prevents a stale generation from being mistaken for current truth.

## Composition

The output is a complete owned generation. Roots are sorted by stable ID. Every
replica resolves to its ultimate root and appears only in that root control's
sorted member-ID list, so a mirror follows its source and never creates a
second slider. If any group member has ambiguous identity, live control remains
possible but `persistenceAllowed` is false for the whole group.

Power values are used only while the owner is present, the complete snapshot
passes Power1 validation, service availability is `Ready` or `Degraded`, and
the relevant capability bit is present. A mapped internal device exposes exact
raw observed/maximum values and a derived 0–10000 integer. Provider degraded or
unavailable status remains typed on that one control. An unmapped external
display, missing device, absent capability, unavailable service, or lost Power
owner remains an honest unavailable display entry; no old raw value is copied.

Keyboard rows are sorted by opaque Power1 handle ID and appear only while the
keyboard-backlight capability is usable. They retain exact raw value/maximum
and `canSet` truth. Their normalized value is derived again from raw bounds; a
cached normalized input is not presentation authority. Losing only keyboard
capability removes only keyboard rows. Losing the fixture owner removes display
controls while leaving independently current keyboard truth; losing the Power
owner removes keyboard rows and makes surviving display controls unavailable,
even if the client wrapper still carries a cached last snapshot.

Invalid fixture or Power input returns a typed error and no partial projection.
An unavailable owner is not an error when represented by its canonical cleared
input. Recomposition from each complete generation naturally removes hotplugged
rows and makes identical inputs equal, allowing a later publisher to preserve
the accepted no-op revision rule.

## Integer conversion

`normalizeRaw(minimum, maximum, value)` and its inverse use rounded 64-bit
integer arithmetic. They accept only `minimum < maximum`, the Power1 raw maximum
bound, an in-range raw value, and normalized integers from 0 through 10000.
There are no floats, curves, lux values, adaptive toggles, or manual-override
policy in this model.

## Mutation and lifetime boundary

The output contains authoritative `current` truth only; no requested-value
field exists. Display mutation remains reserved for PB-5 after D7. Keyboard
mutation remains a Power client operation with typed lineage and no automatic
retry. Rapid-input coalescing belongs to that later client-side mutation path,
not this generation composer. The module owns no QObject, thread, timer, file,
D-Bus connection, Wayland object, platform handle, QML object, or retained
snapshot.

## Focused proof

The candidate selectors are:

```sh
ctest --test-dir build/dev \
  -R '^qindaqt\.brightness-model-' \
  --output-on-failure --no-tests=error
```

The math row covers endpoint, nonzero-minimum, hostile range, monotonicity, and
quantization properties. Composition covers fixture limits/cycles/duplicates,
mirror collapse, ambiguity, raw-derived display and keyboard values, scoped
capability/provider/owner loss, hotplug rebuilding, invalid Power input, and
enumeration independence. A source-policy row rejects Display, D-Bus, QML, and
Qt Quick dependencies in the production module. None is hardware, transport,
service, or UI evidence.
