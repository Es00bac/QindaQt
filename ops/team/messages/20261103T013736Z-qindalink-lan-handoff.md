# QindaLink lane lan: handoff

- Branch link/lan, merged with main 42edc40cb. ADR number used: 0441 (LAN listener and discovery).
- Desktop: LanController, LanSettings, AvahiPublisher/ServicePublisher, Hub unlisten/dropSessions/portOf; Link1 SetLanEnabled and OpenPairingWindow (protected), LanEnabled/LanStatus properties, PairingWindowChanged/LanStatusChanged signals.
- Phone: EndpointConnector/Planner/ManualAddress/Backoff/LanMemory/DiscoveryTable (engine), NsdDiscovery, Connection section in Setup, bridge lan property, EngineState transport (usb/lan/none), USB takeover while on LAN, ACCESS_LOCAL_NETWORK declared.
- Tests: ctest qindaqt.link-* (excluding mirror) 26/26 pass in build/lan, including link-lan (14), link-lan-service, link-avahi (fake daemon, found two marshalling bugs) and link-caller-coverage (SetLanEnabled/OpenPairingWindow denied). phone/engine-tests/run.sh exit 0 (LanTest ok); run-cross-lan.sh OK (real engine vs real service over 127.0.0.2: refused without window, paired with it, transport lan); preview smoke 0 warnings; APK builds; validate-docs and mkdocs strict pass.
- Firewall: read-only only. nftables, firewalld, ufw inactive; qindafirewall.service disabled; kernel ruleset needs root and was not read. No rule changed. User opens inbound TCP 47160 if needed (QindaFirewall app or nft rule).
- Caveats: real Avahi, real NSD, Android 17 local-network enforcement and interface enumeration are live-test items (PM step with user OK; LAN not enabled on the real service). No phone UI interaction performed; APK not installed by this lane.
- Next: PM live Wi-Fi/Tailscale test with user approval; add a desktop-app control for SetLanEnabled/OpenPairingWindow (Link1 client wrapper not done).
