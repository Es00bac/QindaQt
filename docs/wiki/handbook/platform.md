# Platform services and interoperability

Platform services translate system authority into bounded QindaQt values and
operations. Shell applets and Settings routes consume public clients; they do
not directly acquire the privileged or platform-specific backend authority.

| Area | Provider and responsibility | Important limit |
| --- | --- | --- |
| Audio | Audio1 models devices, defaults, streams, volume, and mute through a confined WirePlumber adapter. | QindaQt does not own audio samples or replace PipeWire policy. |
| Display | Display1 models identities/topology and coordinates preview, confirm, revert, journal, and compositor writer. | Nested convergence and physical-output coverage have distinct acceptance gates. |
| Power/brightness | Power1 exposes supply/profile state and coherent bounded brightness; production adapters isolate upstream services. | Session actions and hardware write authority have separate authentication/admission. |
| Network | Network1 exposes secret-free connectivity state, leases, and supported controls behind NetworkManager adaptation. | Credentials belong to a separate interactive secret agent; hardware breadth is not implied. |
| Bluetooth | Bluetooth1 retains inventory/control compatibility; Bluetooth2 adds pairing/trust interactions. | BlueZ owns pairing authority; exact prompt identity matters. |
| Clipboard | A volatile bounded history/capture service supports privacy-gated client interactions. | Default-off history and no disk content history are deliberate privacy limits. |
| Color | Pure validated ICC metadata plus discovery/import and Settings1 assignment intent. | Selecting a profile does not prove compositor application, HDR, or wide-gamut rendering. |
| Fonts | Pure preferences/catalog with a confined fontconfig discovery provider. | Application timing and first-party scope follow the documented composition boundary. |
| Settings | Versioned schema, atomic persistence, complete snapshots, optimistic commits. | Generic accepted keys do not prove every corresponding UI or backend is implemented. |
| Portals | QindaQt's standard Settings appearance backend exports confirmed appearance. | Other portal families use explicitly routed fallbacks rather than fictitious native implementations. |

## Session actions

Logout, lock, suspend, restart, and shutdown span different authorities. Session1
has a shell-PID-authenticated logout contract and fixed teardown order. Other
actions are confined behind their documented authenticated clients and platform
adapters. A power UI should report availability and completion truth rather than
assume every requested action is supported by the current session.

## Interoperability

Wayland is native; rootless XWayland supports legacy clients. Applications use
freedesktop desktop entries, notifications, status-notifier/menu transports, and
portal interfaces where implemented. Focused KDE dependencies are confined and
versioned. No general claim of compatibility with every app, toolkit, portal,
locker, seat, or hardware device follows from one qualified integration test.

For exact current breadth, use [feature status](catalog/features.md). For
service-specific methods, limits, failure handling, architecture decisions, and
focused tests, use the [documentation catalog](catalog/reading.md).
[Architecture](architecture.md), [privacy](privacy.md), and the
[handbook index](index.md) connect these services to the rest of the system.
