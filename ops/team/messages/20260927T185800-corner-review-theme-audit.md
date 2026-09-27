# Shipped theme audit and shadow review checkpoint

Read-only audit at base `46816db4`:

- Eighteen shipped theme JSON documents load from `data/themes`; both slots in `tests/design_tokens/tst_builtin_contrast.cpp` assert sixteen and therefore stop before checking ratios. Remove stale hard-coded count in the implementation lane while preserving nonempty/full-catalog coverage.
- WCAG luminance calculation over all eighteen authored palettes finds default and muted text on canvas, surface and raised surface all at least 4.5:1.
- Concrete unreadable decoration pair: Bliss inactive caption selects `#5F5A4C` on authored inactive title `#7E96B8`, only 2.27:1. `decorationCaptionColor` selects white only by qGray < 128, otherwise it trusts theme text. Contrast-aware fallback needs an implementation owner and focused test.
- Existing controls visual baselines cover seven themes. Existing appearance resolver intentionally substitutes `qinda-light` or `qinda-dark` when the chosen theme is incompatible with explicit mode, while System preserves the selected theme. Eighteen distinct dual-mode designs are not implemented. Audit effective Light/Dark/System palettes separately from original authored palettes.

Static review of exact shadow candidate `419fe44e267a83efc350e30ad783aaac24fe1536` finds no blocking geometry defect. KDecoration documents fixed corner cells, one-axis edge stretching and padding offsets; full-width texture plus one-pixel horizontal center preserves the notch. Vertical center lies below the title. Runtime gates and exact final candidate remain pending, so this is not final acceptance.
