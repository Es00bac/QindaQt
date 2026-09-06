# Artwork and wallpaper runtime replacement candidate

Replacement descendant fixes the rejected Settings Center constructor wiring and adds the separately owned production wallpaper surface. It packages artwork in both SettingsAppearanceRuntime and QindaQt, uses portable `qindaqt:<name>` wallpaper identities, preserves user-layer empty/custom choices over the Jade Fold profile default, reconciles one input-free Background-layer window per screen, and records the boundary in ADR-0078.
