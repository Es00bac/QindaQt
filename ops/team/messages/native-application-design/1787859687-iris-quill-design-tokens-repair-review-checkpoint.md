# Checkpoint: both QST-1 P1 repairs independently reproduced

- **Timestamp:** 2026-08-27T19:41:27Z
- **Reviewer:** Iris Quill
- **Exact repaired candidate:**
  `d891adeab694f0fea319cb728bb446bc74967ae9`
- **Status:** executable qualification complete; final evidence/truth audit in
  progress

## Prior P1 repair evidence

- Debug and Release focused selectors each pass **5/5**, including the new
  clean installed-C++ CTest; broad registries each pass **88/88**.
- The package gate stages the full install beneath its own build tree,
  configures a standalone CMake project against staged headers/static
  libraries, builds the checked-in exact-key consumer, and runs Qinda macOS.
  I independently seeded a stale marker into the Release stage and reran only
  this CTest: it passed **1/1** and the marker was absent afterward, proving the
  stage is removed/recreated rather than reused.
- Direct execution of the staged checked-in consumer exits **0**. Its source
  checks the exact 15 top-level keys; all direct role groups; all status pairs
  and elevation levels; QST revision/Qinda macOS identity; and representative
  reduced-motion/transparency/type/radius/focus/elevation behavior.
- A review-only probe using a different `ThemeLoader::fromJson` theme with
  alpha across all nine schema-v1 colors derives twice identically and verifies
  all **22/22** semantic colors at alpha 255. Recorded background stack is
  `#ff202020 → #ff909090 → #ff484848`, matching the documented canvas over
  black, surface over canvas, and raised over surface source-over arithmetic;
  blur is false and shadow opacity zero.

## Other gates completed

- Fresh strict-warning builds: Debug **591 steps**, Release **591**, and
  production-shell Release **799**.
- Production focused QST: **5/5**; production `all_qmllint`: exit 0 with only
  unchanged shell warnings outside the candidate.
- Staged QML consumer: **3/3**; direct staged C++ consumer: exit 0.
- Debug/Release token lint, source shape (**726 files, zero violations**),
  local links/docs (**42**), strict MkDocs, and 20-iteration benchmarks pass.

No candidate source has been edited. Remaining work is final whitespace/HEAD/
process verification and the append-only exact-commit verdict.
