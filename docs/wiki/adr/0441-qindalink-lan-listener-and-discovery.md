# ADR-0441: QindaLink LAN listener, mDNS advert and phone discovery

- **Status:** Accepted
- **Date:** 2026-11-02
- **Owners:** QindaLink desktop core (`src/services/link_service`), phone engine (`phone/app/android`)
- **Supersedes:** None
- **Superseded by:** None

## Context

[ADR-0434](0434-qindalink-pinned-tls-transport-and-sas-pairing.md) fixed the
trust model for every path and said the LAN listener is off by default, opens
on the same port, and accepts pairing only inside a user-opened 120 s window.
It did not say how the listener is bound, advertised, switched or found. The
phone must also stay linked without a cable, on the home Wi-Fi and over
Tailscale, without weakening any of those rules.

## Decision

1. **Setting.** `lanEnabled` lives in `$XDG_CONFIG_HOME/qindalink/service.json`
   (atomic write, unknown keys preserved, default `false`). `SetLanEnabled` is
   a **protected** Link1 member ([ADR-0438](0438-qindalink-link1-caller-authority.md)):
   it changes what the machine exposes to the network. `OpenPairingWindow`
   (returns the window length, 120 s, or 0 when LAN is off) is protected for the
   same reason. `LanEnabled` and `LanStatus` are open properties; `LanStatus`
   never carries addresses.
2. **Listeners per address.** `LanController` binds one TLS listener per up,
   non-loopback interface address (IPv4, and IPv6 global/ULA; no link-local, no
   container bridges) on the LAN port, and re-syncs every 15 s while enabled so
   Wi-Fi and Tailscale coming and going are followed. It never binds `0.0.0.0` or
   `::`: a wildcard bind also catches `127.0.0.1` and either fails against the
   always-on loopback listener or lets a LAN-classified connection arrive on
   loopback. Bind failures show in `LanStatus` and are retried; they never stop
   the service or the loopback listener.
3. **Origin rules unchanged.** Each listener is tagged `Origin::Lan`. The Hub's
   existing rules apply: LAN pairing only while the window is open, a LAN
   connection never displaces a live loopback one, a loopback connection
   replaces a LAN one, one connection per device. Disabling LAN closes the LAN
   listeners and ends LAN sessions with `bye{shutdown}`.
4. **mDNS.** While at least one LAN listener is bound, the controller publishes
   `_qindalink._tcp` through a `ServicePublisher`. The production implementation
   (`AvahiPublisher`) uses `org.freedesktop.Avahi` `EntryGroup` on the system
   bus, republishes after an Avahi restart, renames on a collision with
   `GetAlternativeServiceName`, and withdraws synchronously (`Free`) when LAN is
   disabled or the service stops. TXT: `proto=1`, `id` = the first 16 hex
   characters of the desktop SPKI digest, `name` = the hostname. The TXT is a
   hint only; the pin is verified in TLS regardless.
5. **Phone connection order.** `127.0.0.1:47160` (USB) first, then the last LAN
   address that worked, then discovered desktops, then the manual address, with
   jittered backoff (each delay in [ladder/2, ladder]). A paired phone tries a
   discovered desktop only if its TXT `id` is a prefix of the pinned SPKI id. An
   unpaired phone tries a discovered desktop only after the user chose it, so it
   never offers pairing to whichever desktop answered first. Discovery
   (`NsdManager`) runs only while no session is up. While on LAN the engine
   checks every 15 s whether USB answers a full handshake and, if so, ends the
   LAN session; the desktop's replacement rule completes the switch.
6. **Truthful transport.** `EngineState` reports `transport`
   (`usb`, `lan` or `none`) from the endpoint that actually connected. The
   manual address is stored in app-private storage and loopback addresses are
   refused there, because a manual loopback endpoint would be reported as LAN
   while really being USB.
7. **Firewall.** The service never changes firewall rules. If a firewall
   blocks TCP 47160 the user opens it (see the
   [architecture page](../architecture/qindalink.md#lan-and-tailscale)).

## Consequences

- LAN exposure needs two deliberate acts: enabling it (a trusted caller) and
  opening a pairing window for each new phone. A paired phone reconnects without
  a window, as designed in ADR-0434.
- The mDNS record and the open port reveal that a QindaLink desktop is present
  while LAN is on, but nothing else: the SPKI prefix is public by nature.
- Tests simulate a LAN address with `127.0.0.2` and an injected address
  provider, and Avahi with a fake daemon on a private bus; the real interface
  enumeration, the real Avahi and Android's NSD have only live checks.

## Revisit when

- Qt or the platform offers interface-change notifications that make the 15 s
  poll unnecessary.
- A Tailscale-aware discovery path (for example MagicDNS service records) makes
  the manual address unnecessary.
