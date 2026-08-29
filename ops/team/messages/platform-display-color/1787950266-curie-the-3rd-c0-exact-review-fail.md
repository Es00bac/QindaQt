# Curie the 3rd — Display Color C0 exact immutable verdict

- Time: 2026-08-28T20:51:06Z
- Verdict: **FAIL**
- Severity: **P0/P1/P2/P3 `0/3/3/1`**
- Candidate: `35a302237403deaf08b29d7879c25b0474a9c310`
- Tree: `0c3dbab0fc2a05973077ad5bbdd6f7ffcda7dd93`
- Parent: `ccec76803d5fba56f991554a0802a2d8b44bb31e`
- Original base: `146fc48358c2659436dec4fc6b6062d23c5ee746`
- Detached worktree: `/mnt/d/QindaQt/worktrees/display-color-c0-review-curie3`
- External mutation artifact: `/mnt/d/QindaQt/reviews/curie-display-color-c0/repro.cpp`

The ADR-0046 correction itself is exact and clean, and every registered
candidate row passes. Integration is nevertheless blocked: an independently
compiled mutation harness reproduces eight concrete contract failures in the
underlying C0 model. The candidate/product tree was never edited.

## Blocking findings

### P1-1 — the lineage fingerprint is neither complete nor unambiguous

`src/services/display_color_model/src/color_validation.cpp:375` through `:414`
concatenates variable strings without domain tags or lengths and hashes only a
small projection of the published catalog/output state. It omits catalog
display name, description, file name, gamut, transfer, header, size and flags;
it also omits capabilities, requested assignment, brightness fields and state
flags. This contradicts `docs/wiki/architecture/display-color-model.md:100`
through `:110`, which makes the fingerprint part of atomic snapshot lineage.

Two direct mutations reproduce this one blocking contract defect:

- two models with the same epoch/revision/profile ID/checksum/origin but
  different published profile display names produce unequal complete
  snapshots and equal fingerprints;
- different catalog tuples `default="a", profileId="bc"` and
  `default="ab", profileId="c"` produce the same fingerprint because both feed
  the same unframed `abc` byte stream.

Repair requires one schema/domain-tagged, length-delimited canonical encoding
that covers every semantically published snapshot field, plus regressions that
mutate every field and require a changed fingerprint.

### P1-2 — resetting to the current epoch regresses lineage

`src/services/display_color_model/src/color_model.cpp:29` through `:34`
unconditionally sets revision to zero even when `newEpoch == serviceEpoch()`.
The harness advances revision to one and calls `resetEpoch("epoch-stable")`;
the same epoch then regresses `1 -> 0`. That violates the documented
model-monotonic revision and exact epoch/revision fence.

Repair must prevent a same-epoch reset (or guarantee a distinct generated
epoch) and pin monotonic same-epoch behavior with a regression.

### P1-3 — the claimed sRGB fallback may apply a BT.2020 profile

`src/services/display_color_model/src/color_model.cpp:53` through `:69` accepts
any existing profile as `defaultSrgbProfileId`; `:262` through `:267` then uses
it for an SDR-sRGB capability clamp. With a one-profile BT.2020 catalog, an HDR
request on SDR-only capabilities publishes policy `SdrSrgb` and profile
`bt2020`. This violates the truthful coherent fallback contract at
`docs/wiki/architecture/display-color-model.md:89` through `:98` and can feed
false applied-color truth into later platform consumers.

Repair must validate the default's sRGB gamut/transfer semantics and define a
fail-closed result when no valid sRGB fallback exists.

### P2-1 — ICC declared/header/descriptor size consistency is not enforced

`src/services/display_color_model/src/color_validation.cpp:138` through `:150`
checks a supplied header buffer against `totalFileSize`, not against its own
ICC declared size. `:225` through `:239` does not require the header's declared
size to equal descriptor `byteSize`. The harness therefore gets `Valid` for
both:

- declared size 128, supplied buffer 256, total file size 512; and
- descriptor byte size 512 with an embedded declared size 128.

This contradicts the exact candidate limits/size-consistency claims at
`docs/wiki/architecture/display-color-model.md:34` through `:45` and `:56`
through `:70`. Repair both comparisons and add mutation-sensitive rows for
declared-smaller, declared-larger and exact equality.

### P2-2 — duplicate normalization depends on input order

`src/services/display_color_model/src/color_validation.cpp:332` through `:370`
keeps the first descriptor for a duplicate ID before sorting. Reversing two
different descriptors with the same ID changes the published catalog. That is
in tension with the same page's order-independent byte-identity claim at
`:72` through `:79`. Either reject conflicting duplicates atomically or choose
a documented canonical winner independent of input order; pin both orders.

### P2-3 — identifier validation exceeds the documented ASCII grammar

`src/services/display_color_model/src/color_validation.cpp:49` through `:59`
uses `QChar::isLetterOrNumber()`, so `écran` is accepted as a stable/profile ID.
The normative table at `docs/wiki/architecture/display-color-model.md:40`
specifies exact `[A-Za-z0-9._:-]`. Implement that ASCII grammar or change the
durable contract after compatibility review; add non-ASCII regressions.

### P3-1 — the handoff overstates direct QtTest checks

Fresh direct executables report 12 header + 12 catalog + 15 model checks =
**39/39**, not the handoff's 15 + 11 + 15 = 41. This does not create a product
failure, but the evidence count must be corrected in the repaired handoff.

## Exact mutation reproduction

Built outside the candidate tree with exact candidate sources:

```sh
c++ -std=c++20 -fPIC -no-pie -Wall -Wextra -Wpedantic \
  /mnt/d/QindaQt/reviews/curie-display-color-c0/repro.cpp \
  src/services/display_color_model/src/color_model.cpp \
  src/services/display_color_model/src/color_validation.cpp \
  -Isrc/services/display_color_model/include \
  $(pkg-config --cflags --libs Qt6Core) \
  -o /mnt/d/QindaQt/reviews/curie-display-color-c0/repro
/mnt/d/QindaQt/reviews/curie-display-color-c0/repro
```

Exit 0; exact output booleans were all `1`:

```text
header_exceeds_declared_accepted=1
descriptor_size_mismatch_accepted=1
same_epoch_revision_regressed=1
changed_snapshot_same_fingerprint=1
ambiguous_encoding_collision=1
duplicate_order_changes_catalog=1
sdr_fallback_uses_bt2020=1
unicode_id_outside_documented_ascii_accepted=1
reproduced=8/8
```

## Passing evidence retained

- Fresh strict Debug configure and complete build: 1253/1253.
- Debug `ctest -R '^qindaqt\.display-color-'`: 6/6, including both source
  policy rows and the staged installed C++ consumer.
- Release relevant target build: 9/9; Release candidate source/policy rows:
  5/5. A broad unrelated Release build was intentionally stopped after
  992/1253 once the immutable FAIL was decided; no full-Release claim is made.
- Direct QtTest: 12/12 header, 12/12 catalog, 15/15 model = 39/39.
- `tools/validate-docs`: 76 documents/navigation; strict MkDocs to
  `/mnt/d/QindaQt/reviews/curie-display-color-c0/site`; source shape 1146/0.
- Boundary poison rejects the planted QtDBus include; Debug staged installed
  consumer configures/builds/runs.
- ADR repair diff is exactly R097 rename plus three reference/nav edits;
  candidate grep finds no ADR-0030 residue.
- Full base range has no deletions and no `ops/team` path. `git diff --check`,
  JSON parsing, `git fsck --no-dangling --no-reflogs`, exact hash resolution,
  and final detached-tree cleanliness all pass.

## Required next action

Do **not** integrate `35a302237403deaf08b29d7879c25b0474a9c310`.
Route these exact findings to Arden Pike for one non-amended descendant that
fixes all eight reproductions, updates tests/docs and corrects the direct-check
count. Curie the 3rd remains the independent reviewer for the exact repaired
descendant; rereview should rerun this same 8/8 harness with every defect
boolean changed to false, alongside the normal gates.
