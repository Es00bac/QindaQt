# QindaQt visual identity

QindaQt's Pearl and Smoked Plum identity pairs warm ceramic surfaces with
rounded plum glass and restrained apricot highlights. Velvet is the dusk
variant. The character comes from shape, material and useful pictograms;
working surfaces remain calm and readable. [ADR-0098](../adr/0098-use-pearl-and-smoked-plum-app-materials.md)
supersedes the former mandatory blue/amber QindaPunk palette.

## Palette and form

| Color | Reference | Role |
| --- | --- | --- |
| Smoked Plum | `#211D27` | Dark canvas and icon outlines |
| Plum glass | `#2B2633` | Raised dark surface |
| Pearl | `#FAF5F0` | Light surfaces and icon paper |
| Apricot | `#EAB391` | Dark-theme action highlight |
| Fired clay | `#8C4D38` | Readable light-theme action color |
| Heather | `#9C86AA` | Layered application icon body |

These are authored theme and artwork values, never per-application palette
literals. Semantic foregrounds, focus, state colors and contrast belong to
[design tokens](../architecture/design-tokens.md). Application content uses
opaque backgrounds. Local translucent gradients, fine rims and rounded layers
suggest glass without sampling wallpaper. Reduced transparency and high
contrast remove these layers; text never fades with its container.

Application icons use tactile rounded silhouettes with pearl marks and a
restrained apricot detail. Navigation commands use recognizable symbolic
geometry with theme-derived tint. Color supplements shape. Places retain short
names; file grids emphasize MIME icons and image previews. Tooltips and accessible
names explain icon buttons without permanent paragraphs of instruction.

A small original glass-folder illustration adds personality to empty directories;
its generation provenance is in `data/artwork/ARTWORK.md` in the source tree.
Mascot artwork is an accent, never a substitute for actionable UI.

One document, directory or shell session occupies one ordinary app window.
QindaQt containers own task grouping, tabs and window splits; first-party apps
do not build parallel task-management systems.

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

The packaged default theme pair uses `qinda-punk.png` as its companion
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
