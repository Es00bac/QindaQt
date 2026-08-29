# Curie the 3rd — Display Color C0 exact-review midpoint

- Time: 2026-08-28T20:44:35Z
- Exact candidate remains: `35a302237403deaf08b29d7879c25b0474a9c310`
- Candidate product tree: clean and immutable
- External review artifact: `/mnt/d/QindaQt/reviews/curie-display-color-c0/repro.cpp`

Static audit found material contradictions between the implemented pure model
and its candidate contract. I compiled the exact candidate sources plus an
external adversarial harness and reproduced all 8 probes (process exit 0):

1. two changed complete snapshots at the same epoch/revision share one lineage
   fingerprint because published fields are omitted from the hash;
2. two different catalog encodings hash identically because variable strings
   have no length/domain framing;
3. `resetEpoch` with the current epoch regresses revision 1 to 0;
4. an ICC header buffer larger than its declared profile size is accepted when
   it stays below a separately supplied file size;
5. descriptor `byteSize` may disagree with the embedded ICC declared size;
6. reversing two distinct duplicate-ID descriptors changes the normalized
   catalog despite the documented order-independent byte identity;
7. the purported default-sRGB fallback may name a BT.2020 profile, producing
   an applied `SdrSrgb` policy with a BT.2020 profile; and
8. Unicode letters pass the identifier validator despite the documented exact
   `[A-Za-z0-9._:-]` grammar.

The harness prints each result and `reproduced=8/8`. These are candidate
defects, not product edits. Fresh Debug configuration/build and all six focused
registered rows are still running; Release, package/policy mutation, docs,
provenance, and cleanliness evidence will complete the exact terminal verdict.
