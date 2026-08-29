# High: opaque D-Bus JSON is expanded before aggregate bounds are enforced

Candidate `00b3d49ac3d7ba94edcf10272fa5e61185d63b56` does not yet satisfy its
hostile-input/bounded-decoder contract. In
`src/services/settings_protocol/src/settings_wire_decode.cpp:18-74`,
`decodeJsonValue()` recursively materializes every child of an opaque `av` or
`a{sv}` while enforcing only depth and entries-per-container. The advertised
4,096-node and 262,144-byte per-value bounds are not evaluated until the fully
expanded value reaches `BoundedSettingsValueCodec::validateValue()` at lines
105-117. A value with many individually legal nested containers can therefore
allocate and traverse far beyond the node/aggregate-byte limits before it is
rejected. The Qt transport also eagerly `qdbus_cast`s a returned top-level map
at `src/services/settings_client/src/qt_settings_transport.cpp:121-127` before
the bounded reply decoder sees it.

This is a blocking denial-of-service/resource-boundary gap because the public
protocol and ADR explicitly promise recursive aggregate limits against hostile
same-session callers/owners. The focused protocol tests only validate already
materialized in-process `QVariant` values; they do not exercise an opaque
`QDBusArgument` whose recursive aggregate crosses a bound during decoding.

Repair should carry a shared usage budget while streaming opaque containers
(including key/string bytes and nodes), stop before appending the child that
crosses a bound, bound the fixed top-level reply map before materializing it,
and add real opaque-argument/private-bus adversarial tests. I am continuing the
rest of the exact-commit review for additional independent findings.
