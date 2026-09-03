# Pinned Controls visual font fixture

The 25 `qindaqt.controls-visual-*` baselines are rendered from exactly the
font bytes in this directory. Do not edit the binaries by hand.

- `NotoSans-Regular.ttf`, `NotoSans-SemiBold.ttf`, `NotoSans-Bold.ttf`,
  `NotoSansMono-Regular.ttf` — copied byte-for-byte from the upstream Noto
  builds documented in `docs/wiki/shell/controls.md` (Copyright 2022 The Noto
  Project Authors, latin-greek-cyrillic release) and then modified only in
  their `name` table: the families were renamed to the repository-owned
  `QindaQt Sans` and `QindaQt Sans Mono` by `rename_family_names.py`. Glyph
  and metrics tables stay byte-identical to upstream (`head` differs only in
  `checkSumAdjustment`, recomputed for the rebuilt container), and no upstream
  build declares a Reserved Font Name, so the renamed files remain
  distributable under `LICENSE-OFL.txt` (SIL Open Font License 1.1).
- The repository-owned family names guarantee the fixture cannot collide with
  or be shadowed by host-installed Noto families; `pinDeterministicFonts()`
  registers them per process, rewrites every theme catalog family onto them
  through pinned runtime theme copies (a `QFont` substitution cannot redirect
  a family the host actually has installed), and fails closed if a file is
  missing, unreadable, checksum-invalid, or declares an unexpected family.
  See ADR-0021 ("Amended", 2026-09-03).
- `rename_family_names.py` recomputes every table directory checksum and
  `head.checkSumAdjustment` per the OpenType specification; the
  `qindaqt.controls-font-pinning` row validates those checksums, so renewing
  the files with a tool that skips that step fails the gate.
- `../fontconfig/no-noto/fonts.conf` is the checked-in host-Noto-hidden
  configuration applied through `FONTCONFIG_FILE` by the
  `qindaqt.controls-visual-no-noto-100-qinda-high-contrast-compact` canary
  row.

To renew the fixture: copy the new upstream TTFs over the four files, run
`python3 rename_family_names.py`, rebuild the Controls tests, regenerate the
baselines with `QINDAQT_UPDATE_CONTROLS_BASELINES=1`, and review the full
baseline diff as a glyph-only change before committing.
