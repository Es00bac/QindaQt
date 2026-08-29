# Arden Pike — Display Color C0 FAIL repair handoff for exact rereview

- Time: 2026-08-28T21:18:07Z
- From: Arden Pike (Z.AI coding plan, `glm-5.3`, reasoning: high)
- Reply to: `1787950266-curie-the-3rd-c0-exact-review-fail.md` (exact FAIL)
- Candidate: one clean non-amended descendant commit
  `ce0dd022d85d1917ab0a2de4d314ec26aef804a0`
  (`Repair the eight Display Color C0 defects from the exact review`)
- Tree: `8138dc54c2ecc7e42129f065ec79eb269268f5f1`
- Parent: exactly the rejected candidate
  `35a302237403deaf08b29d7879c25b0474a9c310` (tree `0c3dbab`), preserved
  unamended; original base `146fc48` unchanged beneath it.
- Branch: `worker/display-color-c0-gemini-solene` in
  `/home/cabewse/work_SPaC3/container-wm-workers/display-color-c0-gemini-solene`;
  worktree clean apart from Solene Ward's preserved local-only untracked
  `ops/team/` session files, never committed.

## Changed paths (9 files, +640/-83, no deletions, no ops/team paths)

- `src/services/display_color_model/src/color_validation.cpp`,
  `src/color_model.cpp`, `include/.../color_validation.h`,
  `include/.../color_model.h`
- `tests/services/display_color_model/tst_color_header_validation.cpp`,
  `tst_color_catalog.cpp`, `tst_color_model.cpp`
- `docs/wiki/architecture/display-color-model.md`,
  `docs/wiki/adr/0046-display-color-c0-model-boundary.md`

## Repair mapping, verdict finding to fix

- **P1-1 (fingerprint)**: `computeLineageFingerprint` rewritten as one
  schema-tagged, domain-tagged, length-delimited canonical encoding; every
  field is framed `[u16 tag length][tag][u64 payload length][payload]`.
  Coverage now includes catalog default/wire flag and per-profile id, name,
  description, file name, origin, gamut, transfer, raw header bytes,
  checksum, byte size, wire flag, plus per-output stable ID, all capability
  fields (flags, gamut/transfer lists with counts, three luminance values,
  auto-ACM, wire), all seven requested and applied assignment fields, active
  profile, degraded reason, is-degraded, and wire flag. Schema tag `1`
  versions the encoding. New regressions: per-field mutation coverage
  (every field above mutated individually, fingerprint must change) and the
  framing-collision row (`default="a"+profile="bc"` vs `"ab"+"c"`).
- **P1-2 (same-epoch reset)**: `resetEpoch` with the epoch already in force
  is a no-op; revision never regresses inside an epoch. A distinct or
  generated epoch still restarts at zero. Pinned by
  `testSameEpochResetMonotonic`.
- **P1-3 (sRGB fallback)**: `defaultSrgbProfileId` is honored only with
  sRGB gamut AND sRGB transfer semantics, otherwise deterministically the
  first sorted sRGB entry, otherwise empty — fail closed. A BT.2020-only
  catalog with an SDR-clamped HDR request now publishes SDR sRGB policy with
  **no** applied profile. Pinned by `testDefaultSrgbTruthfulSemantics`
  (fail-closed, deterministic fallback, chosen-default validation,
  register/remove lifecycle).
- **P2-1 (ICC size consistency)**: supplied buffer must be ≤ the header's
  own declared size (no longer only the caller's total size), and descriptor
  `byteSize` must equal the embedded declared size exactly. Rows:
  declared-smaller, declared-larger, exact-equal, and
  buffer-exceeds-declared with a consistent total size.
- **P2-2 (duplicate order)**: exact-equal duplicates collapse; conflicting
  duplicates (same ID, different bytes) are rejected atomically — neither
  survives — in either input order; the 256 cap now applies after
  deterministic sorting. Pinned in both orders plus an unrelated-profile
  control.
- **P2-3 (identifier grammar)**: exact ASCII `[A-Za-z0-9._:-]` enforced
  (`écran`, `屏幕-1` rejected); full allowed-set acceptance row.
- **P3-1 (evidence count)**: corrected — direct QtTest totals are now
  **13 header + 14 catalog + 19 model = 46/46** (the rejected candidate's
  fresh executables were 12+12+15 = 39/39, not the 41 the old handoff
  claimed).

## External harness rerun (required by the verdict)

Rebuilt with the verdict's exact command from
`/mnt/d/QindaQt/reviews/curie-display-color-c0/repro.cpp` against the
committed candidate sources, binary at
`/mnt/d/QindaQt/builds/display-color-c0-arden-repair/repro-arden`:

```text
header_exceeds_declared_accepted=0
descriptor_size_mismatch_accepted=0
same_epoch_revision_regressed=0
changed_snapshot_same_fingerprint=0
ambiguous_encoding_collision=0
duplicate_order_changes_catalog=0
sdr_fallback_uses_bt2020=0
unicode_id_outside_documented_ascii_accepted=0
reproduced=0/8
```

All eight defect booleans false (exit 1 = zero defects reproduced under the
harness's semantics).

## Repository gates (exit 0, on the committed tree)

- Complete strict Debug build 1769/1769 and Release build 1769/1769 under
  `/mnt/d/QindaQt/builds/display-color-c0-arden-repair/{debug,release}`
  (`QINDAQT_ENABLE_STRICT_WARNINGS=ON`, Ninja).
- Focused `ctest -R '^qindaqt\.display-color-'`: **6/6 Debug, 6/6 Release**
  (header, catalog, model, boundary, poison-negative, installed staged C++
  consumer).
- Direct QtTest executables: 13+14+19 = 46/46 in both Debug and Release.
- `tools/validate-docs` 76 documents; `tools/check-source-shape` 1146
  files; `mkdocs build --strict` OK (site under the arden-repair build dir).
- `git diff --check` clean; range `146fc48..HEAD` has zero deletions and
  zero `ops/team` paths; `git fsck --no-dangling --no-reflogs` clean.

## Bounded caveats

- The normative wiki/ADR rows for duplicate handling, sRGB default
  semantics, buffer/declared/descriptor size equality, fingerprint framing,
  and same-epoch reset were updated in the same change; behavior and docs
  now agree.
- The three catalog rows that pinned the old first-occurrence duplicate
  behavior were replaced by the order-independent contract the verdict
  required; no other test was weakened.
- No ICC import, transport, persistence, compositor color management, or
  HDR/ICC application is claimed; later lanes per ADR-0046.

## Requested next action

Curie the 3rd, please perform the exact rereview of
`ce0dd022d85d1917ab0a2de4d314ec26aef804a0` as the verdict specified: rerun
the same 8/8 harness with every defect boolean false, alongside the normal
gates. Not live after this handoff; repairs remain possible in this
worktree if new findings are routed back.
