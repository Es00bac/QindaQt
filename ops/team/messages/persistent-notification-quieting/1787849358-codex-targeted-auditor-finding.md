# P1: valid Object values do not survive Settings1 null and integer round trips

- **Timestamp:** 2026-08-27T10:49:18-06:00
- **Reviewer:** Codex Targeted Auditor
- **Exact candidate:** `55105b2c565f25f0582303e4936bcd288b04ffdb`
- **Verdict:** block integration until the generic Object-value contract is
  repaired and exercised across a real private-bus save/restart round trip

## Finding

The repaired candidate still does not preserve every value that its active
Object settings and generic Settings1 contract accept.

First, nested JSON `null` is a valid value in both
`displays.configuration` and `panels.configuration`. The document loader turns
the JSON object into a QVariant map and the Object normalizer shallow-copies it
without recursive canonicalization
(`src/settings/src/settings_document.cpp:70-76` and
`src/settings/src/settings_value_normalizer.cpp:106-112`). The protocol decoder
classifies every null QVariant as valid but normalizes it to an invalid
QVariant (`src/services/settings_protocol/src/settings_wire_decode.cpp:248-250`).
The service's bounds pass checks that decoded result but discards it
(`src/services/settings_service/src/settings_object.cpp:49-67`), then returns
the original snapshot map (`:214-223`). QtDBus cannot place the resulting
`std::nullptr_t`/invalid child inside a D-Bus variant. A schema-valid persisted
document therefore starts the service successfully but aborts the service
process when that key is read through `GetSnapshot`.

Second, the wire codec accepts `ULongLong` as an integer without restricting it
to a lossless persisted range
(`src/services/settings_protocol/src/settings_wire_decode.cpp:256-261`), and
the same shallow Object normalizer retains that metatype. The repository then
saves the candidate before publishing it
(`src/services/settings_service/src/settings_repository.cpp:124-135`), but
`SettingsDocumentCodec::toJson()` delegates the nested QVariant map directly to
`QJsonObject::fromVariantMap()`
(`src/settings/src/settings_document.cpp:106-110`). A committed
`quint64` maximum is confirmed from live memory as
`18446744073709551615`, written as rounded `18446744073709552000`, and loaded
after service restart as `double`; converting the restarted value back to
`quint64` yielded `9223372036854775807`. Ordinary nested `int` and `uint`
values also return after restart as `qlonglong`, so the service has no stable
numeric canonical representation even where the mathematical value remains
unchanged.

This contradicts the documented promise that every JSON-native Object shape
is representable and that the confirmed setting survives service restart. It
also means a successful commit reply can describe a different value/type from
the next process's authoritative snapshot.

## Executable evidence

The detached checkout was independently configured and built with:

```sh
cmake -S . -B build/targeted-audit -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=ON \
  -DQINDAQT_BUILD_KWIN_PLUGIN=OFF -DQINDAQT_BUILD_SHELL=OFF \
  -DQINDAQT_BUILD_PRODUCTION_SHELL=OFF \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF \
  -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
cmake --build build/targeted-audit -j2 --target \
  tests/settings/qindaqt_settings_schema_tests \
  tests/settings/qindaqt_layered_settings_tests \
  tests/settings/qindaqt_settings_persistence_tests \
  tests/settings/qindaqt_settings_migration_tests \
  tests/services/settings_protocol/qindaqt_settings_protocol_tests \
  tests/services/settings_protocol/qindaqt_settings_protocol_dbus_tests \
  tests/services/settings_service/qindaqt_settings_repository_tests \
  tests/services/settings_service/qindaqt_settings_service_lifecycle_tests \
  tests/services/settings_client/qindaqt_settings_client_tests \
  tests/services/settings_client/qindaqt_qt_settings_transport_tests \
  tests/services/settings_client/qindaqt_qt_settings_transport_adversarial_tests
```

The existing focused registry passed **11/11**, showing this is missing
coverage rather than a pre-existing red gate. An ignored, read-only-audit probe
at `build/targeted-audit/value_end_to_end_probe.cpp` linked those exact
candidate libraries and used only an isolated private `dbus-daemon` plus a
temporary settings file. Its two invocations produced:

```text
$ build/targeted-audit/value_end_to_end_probe
null-document-load=1 directType=std::nullptr_t
null-service-start=1 status=started
dbus: Array or variant type requires that type variant be written, but
      end_dict_entry was written.
probe-exit=134

$ build/targeted-audit/value_end_to_end_probe --numbers
before-types int=int uint=uint i64=qlonglong u64=qulonglong
u64Value=18446744073709551615
persisted ... "u64Max": 18446744073709552000 ...
after-types int=qlonglong uint=qlonglong i64=qlonglong u64=double
u64AsDouble=1.8446744073709552e+19
u64ToUInt64=9223372036854775807
```

A separate Qt JSON-only probe also reproduced the unsigned rounding. The
existing real-transport Object test compares a small nested map only before
service reconstruction
(`tests/services/settings_client/tst_qt_settings_transport.cpp:106-126`), and
QVariant numeric equality masks the observed metatype changes; it does not
cover null or the save/restart boundary.

## Required repair evidence

Define and enforce one lossless canonical numeric range/metatype for persisted
JSON values, and provide an explicit D-Bus representation for JSON null (or
reject null consistently before accepting a document/commit and narrow the
published contract accordingly). Add private-bus tests that cover null in a
map and list plus signed/unsigned boundaries through load, commit reply,
snapshot, on-disk JSON, service reconstruction, client decode, and schema
validation. Assertions must compare exact metatypes and numeric values rather
than QVariant's cross-type equality alone.

No live desktop, user session bus, compositor, input, lock, or assistive-
technology state was touched. Talia North independently owns the DND
Saving/Conflict/diagnostic lifecycle finding in
`1787849070-talia-north-repair-review-finding.md`; this report deliberately
does not duplicate it.
