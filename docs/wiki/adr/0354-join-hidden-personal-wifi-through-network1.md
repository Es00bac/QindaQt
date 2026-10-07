# ADR-0354: Join hidden personal Wi-Fi through Network1

- Status: Proposed
- Date: 2026-10-07

## Context

Network1 already activates saved Wi-Fi profiles and creates profiles from visible access-point identities. First-use hidden SSIDs cannot be selected by those identities. Settings must offer an ordinary WPA2/WPA3 personal join without acquiring credential authority or making existing strict v1 consumers reject observations.

## Decision

Add the optional method `ConnectHiddenNetwork(epoch, revision, deviceInterface, ssid, security)`. It carries only bounded presentation-safe metadata. SSID text is literal, nonempty valid UTF-8 of at most 32 octets; spaces are significant and are never trimmed. Existing `SecuritySuite::Wpa2Personal` and `Wpa3Personal` are the only accepted values. Unknown enum values, WEP, enterprise and open hidden joins refuse before dispatch.

All public pending/completion values remain existing `ConnectKnownNetwork` operation kind 1. Snapshot/result version, layout, enum domains, capabilities and existing method signatures remain unchanged. The client selects the optional method from its exact typed parameter shape. An old service's definite `UnknownMethod` becomes a local Unsupported result with initiating lineage and a fixed reason. No fallback, automatic replay, or invented remote success is allowed. Other loss/timeout paths remain Uncertain.

The model validates current ready lineage, selected observed available Wi-Fi device, hardware/software radio state, existing profile-control capability and duplicate saved SSID/security identity. The client and service retain one operation at a time. The libnm adapter rechecks the selected device and radio before dispatch, avoiding an unintended first-device fallback.

Hidden partial profiles explicitly set SSID, infrastructure mode and hidden=true. WPA2 uses wpa-psk; WPA3 uses sae. Both pin proto=[rsn], pairwise=[ccmp] and group=[ccmp]; WPA3 additionally requires PMF. No WPA/TKIP or WPA2 fallback is offered for the selected WPA3 choice. The PSK property stays absent and AGENT_OWNED. NetworkManager completes/stores the profile and owns activation through its hidden no-specific-AP path.

Device RSN/CCMP capability preflight is only admission to an attempt. Installed libnm exposes no device-level SAE flag; those bits never claim positive SAE support. NetworkManager/supplicant remains authoritative for SAE/device refusal. Failure details are fixed public reason codes and never raw backend errors. Dispatch success means activation started, not a connected or authenticated network.

The separate secret agent remains the sole credential owner. Presence in Settings only enables attempting its existing path. Each GetSecrets still requires the current NetworkManager unique owner, authoritative Settings.ListConnections membership, bounded allowlisted settings/hints and ALLOW_INTERACTION. No new supplicant authority, global credential lookup or Network1 credential field is introduced. Existing prompt policy bounds supplied PSK/SAE credentials to 64 UTF-8 bytes; this outcome does not claim support for every possible WPA3 passphrase length.

## Consequences and verification

Old-client/new-service snapshots and connection receipts, including while hidden join is pending, must remain strict v1 compatible. New-client/old-service UnknownMethod, stale lineage, radio/device loss, singleflight, cancellation/authority replacement, duplicates, hostile plain-text SSID and 32-byte UTF-8 limits are tested. Serialized profiles must contain the exact anti-downgrade settings and no PSK. Settings contains no password control and consumes only public client/model boundaries. Its new seventh QML component must survive compiled and forced-disk copied-module checks without an installed Network module rescuing it.

No live network or credential action is needed for synthetic/native qualification. Physical hidden WPA2 and WPA3 association remains a later explicitly authorized installed-session gate. Historical r16 source, recipe and package remain immutable; this change requires a future reviewed delivery.

## References

- [Network1 v1](../reference/network1-v1.md)
- [Network service](../architecture/network-service.md)
- [Secret agent](../architecture/network-secret-agent.md)
- [Network Settings](../apps/network-settings.md)
- [NetworkManager wireless security settings](https://networkmanager.dev/docs/api/latest/settings-802-11-wireless-security.html)
- [libnm asynchronous activation](https://networkmanager.dev/docs/libnm/latest/NMClient.html)
- [NetworkManager hidden profile completion source](https://github.com/NetworkManager/NetworkManager/blob/main/src/core/devices/wifi/nm-device-wifi.c)
