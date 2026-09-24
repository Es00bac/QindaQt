# Claim: W5 Network panel applet

- Worker: claude-w5-network-applet
- Time: 2026-09-23T23:53:43-06:00
- Base: 2e415cad
- Branch: worker/claude-w5-network-applet-20260923
- Outcome: panel applet `qindaqt.applets.network` over the public NetworkClient
  (status glyph, popup with Wi-Fi switch, connections with Disconnect, visible
  networks with join, Rescan, Network Settings link), System Status network
  lane, manifest/registry/profile wiring, ADR-0258, wiki page.
- Owned paths: src/shell/network_applet/**, src/shell/runtime/networkappletcomposition.*,
  tests/shell/network_applet/**, data/applets/network.json,
  docs/wiki/shell/network-applet.md, docs/wiki/adr/0258-*.md.
- Shared additive edits: applet capability enum (network.read/network.control),
  builtin registry, shell runtime wiring and panel QML access chain, shell and
  test CMake, qindaqt.json profile, system-status manifest and desktop_controls
  System Status, icon coverage fixture, catalog/resolver tests, shell wiki pages,
  mkdocs.yml.
