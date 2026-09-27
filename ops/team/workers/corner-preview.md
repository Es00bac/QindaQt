# Corner preview

- Status: working — make Settings preview preserve the live title cutout and contour shadow.
- Base: 926bfc6a
- Branch: fix/corner-preview-silhouette
- Owned paths: Appearance window preview split, pure shared shadow texture/painter, KDecoration shadow adapter extraction, focused tests/docs.

## Updates

- 2026-09-27T19:44:00Z: Claimed isolated preview defect. Full-frame client fill makes the short title cutout opaque; no shadow was painted. Manager approved extracting unchanged production shadow texture generation into the shared pure painter, keeping KDecoration as adapter and adding exact preview nine-patch rendering.
