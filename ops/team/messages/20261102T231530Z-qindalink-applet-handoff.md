# QindaLink applet handoff

- Lane: applet, branch `link/applet`, base `709e7d423`.
- Commit: see the final line of the lane report (the handoff file is part of the branch tip; the first slice is `794753f76`).

## What landed

`qindaqt.applets.link`: pure five-state projection, runtime controller,
`LinkApplet.qml` (+ `LinkPairingSection.qml`), composition over the public
`LinkClient`, `phone.read` / `phone.control` capability tokens, manifest
`data/applets/link.json`, policy rows, registry entry, panel access threading,
flagship placement (`data/profiles/qindaqt.json`, instruments panel), stub
module, `phone(-symbolic)` glyphs in `data/icons/QindaQt`, docs
(`docs/wiki/shell/link-applet.md`, ADR-0435, mkdocs, ADR index, architecture
and manifest-schema pages).

## Changed outside the applet (review these)

- `src/services/link_client`: `LinkDevice::capabilities`, parsed from an
  optional `caps` array (absent = empty). Additive, no Link1 change. **Link1
  does not emit `caps` yet**, so Ring phone stays disabled; the service needs to
  add `caps` to the device snapshot (link_service lane / PM decision).
- Pre-existing red rows repaired because they block the stub requirement:
  `tests/shell/qml/imports` had no `FirewallApplet` / `AgentUsageApplet` stubs
  and the `AudioApplet` stub lacked `desktopControls`; added.
  `tests/applets/tst_manifest.cpp` expected 35 manifests at base (stale: 36
  existed), now 37. The status-notifier and clipboard panel-host harnesses did
  not link the AgentUsage / Network / Firewall runtime plugins; added those and
  Link.
- `tests/session/DesktopVirtualAppletModules.cmake` stages the Link module.
  (Firewall is still missing there; not touched.)

## Tests (build root `build/applet`, plain ninja/ctest, exit 0)

- `qindaqt.link-applet-presentation`, `-controller`, `-offscreen`, `-boundary`: 4/4.
- `qindaqt.link-client` (new caps parse case): pass.
- Count guards: `qindaqt.applet-manifest` (37), `qindaqt.applet-catalog` (37),
  `qindaqt.applet-runtime-resolution`: pass.
- `qindaqt.shell-panel-access-threading`, `qindaqt.shell-icon-coverage`,
  `qindaqt.profile-formats` (new "only flagship places link"),
  `qindaqt.shell-runtime-panel-applet-compatibility`: pass.
- Stub-protected dispatcher/panel rows (all pass): `qindaqt.launcher-panel-dispatcher`,
  `qindaqt.task-list-applet-panel-dispatcher`, `qindaqt.panel-geometry-offscreen`,
  `qindaqt.panel-zone-budget-offscreen`, `qindaqt.luna-taskbar-offscreen`,
  `qindaqt.notification-center-applet-offscreen`,
  `qindaqt.live-customization-offscreen`,
  `qindaqt.live-customization-edit-mode-offscreen`,
  `qindaqt.global-menu-production-panel-keyboard-qml-offscreen`,
  `qindaqt.status-notifier-applet-production-panel-keyboard-offscreen`,
  `qindaqt.clipboard-applet-production-panel-keyboard-offscreen`,
  `qindaqt.clipboard-applet-production-panel-return-offscreen`.
- `mkdocs build --strict` ok; `tools/docs_validation.py` ok (590 docs);
  `tools/check-source-shape`: no error for link files (LinkApplet.qml 298
  non-blank lines, a decomposition-review warning at 275).

## Caveats

- No live check: no service, phone or portal was touched.
- ADR number 0435 was free on main when written; renumber at integration if
  another lane took it.
- A pairing request raised before the shell starts is not shown (Link1 has no
  pending-pairing property); the service notification still carries it.
- "Open QindaLink" stays hidden until an `org.qindaqt.Link.desktop` entry is
  installed (the app lane).

## Next action

Integrate; ask link_service to add `caps` to the Link1 device snapshot.
