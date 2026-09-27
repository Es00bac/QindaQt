# Corner preview

- Status: idle — preview silhouette candidate handed off for native review and delivery.
- Base: 926bfc6a
- Branch: fix/corner-preview-silhouette
- Owned paths: Appearance window preview split, pure shared shadow texture/painter, KDecoration shadow adapter extraction, focused tests/docs.

## Updates

- 2026-09-27T19:44:00Z: Claimed isolated preview defect. Full-frame client fill makes the short title cutout opaque; no shadow was painted. Manager approved extracting unchanged production shadow texture generation into the shared pure painter, keeping KDecoration as adapter and adding exact preview nine-patch rendering.

- 2026-09-27T19:53:28Z: Corrected native-capture finding: final decoration paint now excludes the floating-point client rectangle. Local silhouette gate passes 6/6, including below-seam client-color assertions for rounded Corner Bar at 1x/2x and ordinary titles. Earlier extraction comparison proved 64/64 production shadow textures identical. Manager owns native rerun and both-host delivery; reviewer receives exact candidate.
