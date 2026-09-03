# Pinned Controls visual font fixture

The 25 `qindaqt.controls-visual-*` baselines are rendered from exactly the
font bytes in this directory. Do not edit the binaries by hand.

- `NotoSans-Regular.ttf`, `NotoSans-SemiBold.ttf`, `NotoSans-Bold.ttf`,
  `NotoSansMono-Regular.ttf` — copied byte-for-byte from the upstream Noto
  builds documented in `docs/wiki/shell/controls.md` (Copyright 2022 The Noto
  Project Authors, latin-greek-cyrillic release) and then modified only in
  their `name` table: the families were renamed to the repository-owned
  `QindaQt Sans` and `QindaQt Sans Mono` by `rename_family_names.py`. Glyph
  and metrics tables stay byte-identical to upstream, and no upstream build
  declares a Reserved Font Name, so the renamed files remain distributable
  under `LICENSE-OFL.txt` (SIL Open Font License 1.1).
- The repository-owned family names guarantee the fixture cannot collide with
  or be shadowed by host-installed Noto families; `pinDeterministicFonts()`
  registers them per process and fails closed if a file is missing,
  unreadable, or declares an unexpected family. See ADR-0021 ("Amended").

To renew the fixture: copy the new upstream TTFs over the four files, run
`python3 rename_family_names.py`, rebuild the Controls tests, regenerate the
baselines with `QINDAQT_UPDATE_CONTROLS_BASELINES=1`, and review the full
baseline diff as a glyph-only change before committing.
