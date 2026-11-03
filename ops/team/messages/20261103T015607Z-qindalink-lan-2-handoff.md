# QindaLink lane lan-2: handoff (security P1s and P2s)

- Branch link/lan-2 from main cdb866103; ADR-0441 amended (no new number).
- P1.1 starvation: per-origin unauthenticated caps (loopback 8; LAN 3 total, 1 per source), pinned ids exempt, per-source and global accept budget before TLS (AcceptLimiter, 6/30 per 10 s), 3 s LAN handshake timeout, USB pairing preempts a LAN pairing holding the slot.
- P1.2 scope: private ranges only by default (RFC1918, 100.64/10, fc00::/7); protected SetLanInterfaces allowlist in service.json; LanStatus reports interface names (no addresses); Avahi AddService per bound interface only (unresolvable names publish nothing). A private-range hotspot is still included unless the allowlist is used (documented).
- P1.3 window: first accepted LAN pair.request closes the window; a timed-out window aborts in-flight LAN pairings; PendingPairings gains origin/source, Hub pairingRequested and the notification say "over Wi-Fi from <ip>". D-Bus PairingRequested signature unchanged; app/applet consumers may read PendingPairings origin (documented in link1-v1.md; consumers not changed).
- P2: phone EndpointConnector checks the presented key against the pin and moves on (also for the USB upgrade probe); ManualAddress rejects IPv4-mapped loopback; --lan-port/--lan-test-address/--pairing-window-ms are TEST/DEVELOPMENT ONLY with a warning; ADR states the mDNS beacon and the cap interaction.
- Tests: qindaqt_link_lan_test (22 cases incl. flood vs pinned reconnect and USB pairing, per-source cap, accept limit, allowlist, window consumption, origin), link-avahi (interface-bound announce), caller-coverage (SetLanInterfaces denied), phone LanTest (rogue endpoint, mapped loopback).
- No live LAN, no firewall change, no phone UI.
