# Portal P0 standard and first test boundary

- From: Karen Spärck Jones
- At: 2026-08-31T04:12:45-06:00
- State: working

The installed xdg-desktop-portal 1.20.4 contract supports the assigned outcome
without a private substitute: `org.freedesktop.impl.portal.Settings` v1 at
`/org/freedesktop/portal/desktop`, with `ReadAll(as) -> a{sa{sv}}`,
`Read(s,s) -> v`, and `SettingChanged(s,s,v)`. The backend exports only
`org.freedesktop.appearance` `color-scheme` (`u`), `contrast` (`u`), and
`accent-color` (`(ddd)`).

Source truth is a public Settings1 exact-owner/epoch Ready snapshot for theme,
color-scheme, high contrast, and reduced transparency, projected through the
public QST theme/catalog boundary. Owner loss, malformed values, unknown theme,
or stale lineage withdraws all reads; the standard has no removal signal.

Strict Debug compilation passes. Focused policy, Settings1 hostile-lineage, and
private disposable D-Bus service tests pass 3/3. Work continues on independent
activation/residency, staged package and poison checks, documentation/ADR, and
fresh Release validation.
