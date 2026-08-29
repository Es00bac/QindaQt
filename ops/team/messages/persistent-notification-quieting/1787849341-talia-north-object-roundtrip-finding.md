# P1: generic Object null crashes QtDBus marshalling and wide unsigned values corrupt on restart

- **Timestamp:** 2026-08-27T10:49:01-06:00
- **Exact candidate:** `55105b2c565f25f0582303e4936bcd288b04ffdb`
- **Severity:** P1 — blocks integration

The repaired protocol still cannot carry every JSON-native value that its
public contract accepts, and the model/persistence boundary does not preserve
the accepted numeric domain.

`decodeJsonValue()` explicitly accepts invalid/null `QVariant` and every
`UInt`/`ULongLong` at
`src/services/settings_protocol/src/settings_wire_decode.cpp:248-260`, while
the active schema's Object normalizer passes an entire `QVariantMap` through
unchanged at `src/settings/src/settings_value_normalizer.cpp:106-112`.
`SettingsClient::setUserValue()` then places that value directly into a nested
QtDBus `QVariantMap`/`QVariantList` operation at
`src/services/settings_client/src/settings_client.cpp:181-207`; there is no
wire encoder for JSON null or canonical numeric representation.

An isolated private-`dbus-daemon` probe against the exact built hash committed
`displays.configuration={"null": QVariant{}}`. The sending process aborted in
libdbus marshalling with exit 134:

`Array or variant type requires that type variant be written, but end_dict_entry was written.`

D-Bus variants have no untyped null payload, so accepting null in the local
codec is not proof it can cross this wire. The same shape loaded from a user
JSON document can make a snapshot reply hit the corresponding service-side
marshaller path.

A second private-bus probe skipped null and committed `u32=4000000000` plus
`u64=UINT64_MAX`. Before restart, the authoritative snapshot returned `uint`
and `qulonglong` with exact values. `SettingsDocumentCodec::toJson()` delegates
the pass-through Object to `QJsonObject::fromVariantMap()` at
`src/settings/src/settings_document.cpp:94-110`; the file encoded `u64` as
`18446744073709552000`. After reconstructing `ResidentSettingsService` from
that file, the snapshot returned `u32` as `qlonglong` and `u64` as `double`; the
wide value was no longer the committed integer. Both service starts and the
unsigned commit itself reported success, so this is silent persisted-state
corruption rather than a clean validation failure.

The in-process protocol test at
`tests/services/settings_protocol/tst_settings_protocol.cpp:20-37` includes a
null but never sends it; the real private-bus nested Object test at
`tests/services/settings_client/tst_qt_settings_transport.cpp:106-126` uses
only small signed integers. Repair needs one explicit symmetric wire encoding
for null, recursive model/persistence normalization for every Object child, and
a documented exact integer range/canonical type that cannot change or round on
save/restart. Add real private-bus set/snapshot and service-reconstruction tests
covering null in map/list positions, boundary signed/unsigned numbers, and
clean rejection beyond the supported integer domain. Do not merely remove the
existing in-process null assertion while continuing to claim every JSON-native
shape.

The probe lived only under the ignored reviewer build directory. Candidate
source remained unmodified; no live desktop, real session bus, or input was
used.
