# QindaQt visual identity

QindaQt's **QindaPunk** identity combines a cinematic ink-blue night, graphite
surfaces, pale porcelain text and a restrained amber action color. It should
feel precise and characterful while leaving the wallpaper's quiet areas usable
for work. Artwork supports readable controls and an uncluttered workspace.

## Palette and form

| Color | Reference | Role |
| --- | --- | --- |
| Ink night | `#111E2C` | Dark grounding and strong silhouettes |
| Graphite | `#192939` | Nightfall application surfaces |
| Porcelain | `#F5F7F3` | Light counterpart surfaces |
| Amber | `#D98A32` | Action and selected-state emphasis |
| Burnt amber | `#9B4D12` | Light-theme action color with readable text |
| Ice blue | `#83BDF2` | Status/info and focus fallback only |

The Nightfall and Porcelain values above are the packaged `qinda-dark` and
`qinda-light` semantic theme roles. Ice blue is supplied by QST status/info
derivation and is not a general-purpose accent. UI foregrounds, state colors,
high contrast, motion preferences, and semantic contrast rules remain owned by
[design tokens](../architecture/design-tokens.md).

Application icons use rounded silhouettes and a few clear layers. First-party
application icons draw a blue body with at most one amber accent; jade appears
only for success or positive status, never as a brand color (see
[Icon theme](icon-theme.md)). Action and
status icons must communicate their meaning at small sizes without relying on
color. Different battery levels, navigation directions, and destructive actions
must remain distinguishable. The same rounded vocabulary connects the icons
to the dock; detailed texture belongs in the wallpaper rather than tiny controls.

## Wallpaper collection

| Wallpaper | Character |
| --- | --- |
| Jade Fold | Sea-glass folds, a warm horizon, and the former Mineral Light palette |
| Porcelain Dawn | Pale ceramic curves and diffuse apricot daylight |
| Ink Tide | Smoked glass, deep ink-blue, and restrained violet edge light |
| Qinda Punk | A chibi cyborg penguin, rooftop neon, coffee, and a rubber-duck drone |
| Compile Club | The same mascot in a warm hacker workshop; the duck cools a compiling machine |

All five leave quiet areas for windows and icons. They share a material and
shape vocabulary rather than merely recoloring one image. The original PNGs
are 1672 × 941 pixels. Generation provenance and full prompts are preserved
with the assets in `data/wallpapers/ARTWORK.md`.

The packaged QindaPunk theme pair uses `qinda-punk.png` as its companion
wallpaper when the default appearance has no explicit wallpaper choice. Use
[Appearance Settings](../apps/appearance-settings.md) to select artwork. The
desktop preserves an explicit user choice, including no wallpaper.
Do not make this visual identity a reason to restrict custom themes, images,
or alternative layout profiles.

The **QindaPunk** mascot adds QindaHumor: cute proportions, serious cybernetic
gear, and affectionate Linux jokes. Keep the penguin recognizable through its
jade visor, amber boots, Q patch, and determined expression. The rubber-duck
sidekick carries the visual joke. Artwork should remain readable as a desktop
background, with the characters offset from the main working area.
