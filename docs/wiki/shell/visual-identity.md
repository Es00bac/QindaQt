# QindaQt visual identity

QindaQt's **Mineral Light** identity combines soft geometry, mineral colors,
and warm light. It should feel calm, friendly, and precise. Artwork supports
readable controls and an uncluttered workspace.

## Palette and form

| Color | Reference | Role |
| --- | --- | --- |
| Ink | `#172528` | Dark grounding and strong silhouettes |
| Jade | `#70BFA5` | Signature color and gentle emphasis |
| Porcelain | `#F3EFE5` | Warm light surfaces |
| Apricot | `#E8AE84` | Warm contrast and small accents |
| Blue | `#739EBB` | Cool secondary accent |
| Violet | `#A69AC5` | Restrained complementary accent |

These are artwork colors, not a replacement for semantic QST contrast rules.
UI foregrounds, state colors, high contrast, and motion preferences remain
owned by [design tokens](../architecture/design-tokens.md).

Application icons use rounded silhouettes and a few clear layers. Action and
status icons must communicate their meaning at small sizes without relying on
color. Different battery levels, navigation directions, and destructive actions
must remain distinguishable. The same rounded vocabulary connects the icons
to the dock; detailed texture belongs in the wallpaper rather than tiny controls.

## Wallpaper collection

| Wallpaper | Character |
| --- | --- |
| Jade Fold | Sea-glass folds, a warm horizon, and the signature jade palette |
| Porcelain Dawn | Pale ceramic curves and diffuse apricot daylight |
| Ink Tide | Smoked glass, deep teal, and restrained violet edge light |
| Qinda Punk | A chibi cyborg penguin, rooftop neon, coffee, and a rubber-duck drone |
| Compile Club | The same mascot in a warm hacker workshop; the duck cools a compiling machine |

All five leave quiet areas for windows and icons. They share a material and
shape vocabulary rather than merely recoloring one image. The original PNGs
are 1672 × 941 pixels. Generation provenance and full prompts are preserved
with the assets in `data/wallpapers/ARTWORK.md`.

Use [Appearance Settings](../apps/appearance-settings.md) to select artwork.
The desktop should preserve an explicit user choice, including no wallpaper.
Do not make this visual identity a reason to restrict custom themes, images,
or alternative layout profiles.

The **QindaPunk** mascot adds QindaHumor: cute proportions, serious cybernetic
gear, and affectionate Linux jokes. Keep the penguin recognizable through its
jade visor, amber boots, Q patch, and determined expression. The rubber-duck
sidekick carries the visual joke. Artwork should remain readable as a desktop
background, with the characters offset from the main working area.
