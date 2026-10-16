# ADR-0393: Merge every XDG data directory for shell catalogs

- Status: Accepted
- Date: 2026-10-16

## Context

The shell resolved its theme, applet-manifest and applet-policy catalogs with
`QStandardPaths::locate`, which answers with the **first** match. A stale
`~/.local/share/qindaqt/themes` (19 symlinks left from September, none of them
the user's saved theme) therefore hid all 28 installed themes, and the saved
theme was rejected at every login with a warning that named only the requested
theme while claiming to name the fallback. Settings, the profile catalog and the
layout store already merged every data directory with the user's last; the shell
was the outlier, which is why Settings could show a theme the shell refused.

## Decision

The shell's theme, applet and applet-policy catalogs merge every XDG data
directory in **low-to-high precedence**: the source tree only for a catalog the
caller names (build-tree executable only), then the system roots in reverse
`XDG_DATA_DIRS` order, then `XDG_DATA_HOME` last. A later entry overrides an
earlier one by id. An explicit path or an environment override is the whole
answer and is never merged (isolated, as for profiles).

- The **base layer is strict**: an invalid entry in the lowest layer fails the
  load, as before, because the installed catalog is the product.
- **Override layers are lenient**: an invalid or oversized entry in a higher
  layer is skipped and reported through the diagnostics list; the installed entry
  stays in force and the shell logs one warning per skipped entry.
- **Policy merges by selector**: a higher layer overrides a lower layer's
  decision for the same selector and never deletes one. This is more
  restrictive than the old shadowing, where a user policy file replaced the
  system file entirely and could drop its denials.
- The saved-theme fallback warning names the requested theme, the fallback
  theme and the directories searched.
- The shell logs the merged directory lists at startup, low to high.

## Consequences

A user directory holding a stale subset can no longer hide installed entries; a
user entry overrides an installed one with the same id. The legacy first-match
resolvers remain only for the preview tool, which presents one catalog
directory by design. Tests: `tst_catalogpaths` (precedence and isolation),
`tst_theme_catalog_merge`, `tst_catalog_merge` (applets), `tst_policy_merge` and
`qindaqt.shell-catalog-merge`, which runs the real `qindaqt-shell --list` with a
user directory holding a stale subset. See
[Layout profiles](../shell/layout-profiles.md) ("Catalog directories").

## Revisit when

A catalog needs deletion by a higher layer (a tombstone), which this decision
deliberately does not allow for policy.
