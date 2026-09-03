# Vendored font fixtures

These fonts are vendored test fixtures for the fontconfig discovery provider
rows under `qindaqt.font-discovery-*`. They are copied byte-for-byte from
upstream releases and are each licensed under the SIL Open Font License,
Version 1.1:

| File | Upstream source | License |
| --- | --- | --- |
| `NotoSansLycian-Regular.ttf` | Noto Sans Lycian (github.com/notofonts / googlefonts/noto-fonts) | SIL OFL 1.1 |
| `NotoSansOgham-Regular.ttf` | Noto Sans Ogham (github.com/notofonts / googlefonts/noto-fonts) | SIL OFL 1.1 |
| `LiberationMono-Regular.ttf` | Liberation Fonts 2.x (github.com/liberationfonts/liberation-fonts) | SIL OFL 1.1 |

The full OFL 1.1 text is available at
https://openfontlicense.org/open-font-license-official-text/ and is embedded in
each font's name table. Do not edit the font bytes; discovery tests assert
exact family names, styles, weights, and spacing flags read from them.
