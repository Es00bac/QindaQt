# Annie Easley — Bluetooth B1 meta-object repair Debug checkpoint

- Timestamp: 2026-09-02T20:50:57-06:00
- Exact base: `35f2fa20881437fc3ef9d85ce399dc68e12ed1d3`
- Status: working

The runtime textual gate now owns only the seven-file inventory, exact include
allowlist, forbidden-symbol checks, and production composition-token checks.
Its full direct run exits 0 with five independent include/forbidden-symbol
poison rejections; the prior normalization, property/invokable regexes, literal
counts, and six lexical-surface poisons are gone.

The new compiled test walks the controller-owned post-moc property, method, and
enumerator slices in order against literal full-attribute contracts. A
test-local derived type adds a property, slot, signal, and enum; the same
comparison function reports every category while its empty base matches an
empty contract. The test also constructs the production `BluetoothApplet`
through an offscreen `QQmlEngine`, reflects controller and plain-`QObject`
members in QML, subtracts the inherited baseline, and requires the remaining
property/method names to equal the literal compiled contract.

Strict GCC 15.3 Debug configured from the assigned initial cache. The exact
focused graph built 319/319 actions after two isolated new-test red/repair
cycles (overload disambiguation, then moc's canonical `qulonglong` type name
and a QQuickItem property-name collision). The complete Bluetooth selector
passes 8/8 and the exact adjacent selector passes 6/6. No private bus, host
desktop, BlueZ, radio, hardware, network, input, or nested compositor was used.
