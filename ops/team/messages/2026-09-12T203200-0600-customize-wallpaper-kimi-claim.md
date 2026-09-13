# Customize wallpaper preview — claim (Kimi, Files rotation)

- 2026-09-12T20:32:00-06:00 — Claimed the configured-wallpaper CustomizeCanvas
  outcome from exact base `6d05caa216ae5dc4db7fe92d6109491d02e5c627` in worktree
  `.cache/small-team/worktrees/customize-wallpaper-preview` (branch
  `worker/small-customize-wallpaper-preview-20260912`). Verified base, clean
  tree, scope against the September 12 scope guard (Appearance preview work is
  authorized), and no path collision with the active Dock/Containers/
  Files-Desktop/Review lanes.

Material fact: the public boundary is sufficient — the Customize route can
reuse `qindaqt/apps/settings_appearance/appearance_values.h` (wallpaper keys,
`wallpaperModeFromToken`) and `wallpaper_catalog.h`
(`discoverBundledWallpapers`, the `qindaqt:<basename>` identity→path mapping
with first-root-wins order) together with its own Settings1 client scoped to
`appearance.wallpaper`/`appearance.wallpaperMode`. The shell runtime's
`resolveWallpaperSource` remains private and untouched; no service,
framework, or duplicate resolver is added. The route boundary allowlist test
gains the one new public prefix in the same change, with the wiki dependency
paragraph updated alongside.

Worker record: `ops/team/workers/customize-wallpaper-kimi.md` (status:
working). `6d05caa2` stays an immutable independent-review candidate; this
work is a descendant on the assigned branch.
