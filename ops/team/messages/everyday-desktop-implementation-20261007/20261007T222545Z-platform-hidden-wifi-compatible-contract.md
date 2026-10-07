# ED08 revised compatible hidden-join contract

This addendum supersedes the new OperationKind5 proposal in 20261007T222007Z-platform-hidden-wifi-boundary-proposal.md. Product files remain unchanged; compiler idle.

## Wire and public boundary
Network1.ConnectHiddenNetwork(epoch t, revision t, deviceInterface s, ssid s, security u)->ay is an optional additive method. All public pending/results remain existing OperationKind::ConnectKnownNetwork(1). No enum-domain, Capability, snapshot/result codec layout/version, or existing method change. Therefore network_types.h and network_validation.cpp leave requested ownership. Add ConnectHiddenIntent in network_intent.h; security uses existing SecuritySuite values, ONLY WPA2Personal/WPA3Personal admitted, all unknown values refused. Exact SSID printable valid Unicode/UTF8 1..32 octets, no trim, preserve spaces and exact encoding; selected normalized observed WiFi interface required.

Append default-empty optional typed hidden intent to service/backend request DTOs. Existing known-profile activation remains default path; hidden request branches privately through model policy and adapter. Client convenience and transport use exact secret-free parameter shape for new RPC, existing Connect kind. Definite new-method UnknownMethod finishes local Unsupported with fixed hidden-network-control-unsupported reason, no invented remote receipt/retry. All other transport uncertainty retains existing conservative contract. Existing snapshots/results stay decodable by strict old clients even during hidden join. Required old-client/new-service active-operation golden and new-client/old-service UnknownMethod fixtures.

Ready/current lineage, singleflight, radio hardware/software, observed selected WiFi device and existing known-profile capability gate. Reject duplicate exact knownNetworkId(SSID,security) rather than replacing stored profiles. No credentials in any API/UI/result/log; secure join admission uses existing presence-only secret-agent observation; same credential authority and GetSecrets ownership unchanged.

## Verified platform boundary and unresolved SAE admission
Installed libnm nm-dbus-interface.h defines RSN/CCMP device capabilities, but NO NM_WIFI_DEVICE_CAP_SAE. SAE flag belongs to AP security, which is absent for a hidden first join. Do not invent a device capability. Selected device RSN/CCMP preflight and fixed authoritative NM refusal are possible; positive SAE support cannot honestly be inferred from those bits. Root/Astra contract decision requested for exact SAE admission/refusal before implementation. No private supplicant introspection/new authority proposed.

Official upstream nm-device-wifi.c complete_connection accepts null specific_object with nonempty explicit wireless SSID, verifies settings if no compatible AP, and marks hidden. The factory sets mode infra/hidden=true/key-mgmt wpa-psk or sae, PSK absent/AGENT_OWNED; hidden SAE PMF-required and RSN settings will be verified using libnm settings tests. NetworkManager owns activation and asynchronous refusal; callback success is dispatch, not finished connection.

## Exact ownership delta
Retain source/test/docs path list in prior proposal except remove network_types.h/network_validation.cpp. Add network_identity.h comment clarification only: known-profile intents use derived identity; hidden-first-use intent explicitly carries bounded SSID metadata. Reserve ADR0354 per manager. No production secret-agent edits or root registries. New private hidden adapter and Settings collaborators keep large current files below decomposition threshold. Owning CMake source/QML/test additions only; future disk module gets seven files, immutable r16 remains unchanged.

Primary references:
- https://networkmanager.dev/docs/libnm/latest/NMClient.html
- https://networkmanager.dev/docs/api/latest/nm-dbus-types.html
- https://raw.githubusercontent.com/NetworkManager/NetworkManager/main/src/core/devices/wifi/nm-device-wifi.c
- https://networkmanager.dev/docs/api/latest/nm-settings-nmcli.html

Next: root/Astra exact path/SAE contract response, then source-only implementation; native/private gates await lease. No live network/radio actions.
