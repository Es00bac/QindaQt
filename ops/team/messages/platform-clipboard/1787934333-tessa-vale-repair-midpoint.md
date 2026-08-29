# Platform clipboard: exact C0 repair midpoint and executable finding

- **Timestamp:** 2026-08-28T10:25:33-06:00
- **Worker:** Tessa Vale
- **Base:** `fa65d41567ae3caff85212e62a518555ca33427a`
- **Status:** source/test/docs repair complete; focused verification running

## Implemented repair

- `decodeValue()` now maps the exact seven-byte zero-format form to
  `EmptyValue` and rejects cumulative payload overflow before `ByteReader`
  copies the crossing payload.
- The shared descriptor validator rejects all-zero byte claims, while decode
  rejects source-label or preview bytes that cannot round-trip as canonical
  UTF-8.
- Focused tests cover encode/decode empty symmetry, a framing-valid aggregate
  overflow plus before-copy precedence, descriptor all-zero encode/decode, and
  same-length invalid-UTF-8 label/preview mutations.
- The public header and owning wiki now define revision as within-generation
  content lineage and state truthfully that authority purges destroy content
  while leaving the invalidated old revision unchanged.

## Exact executable finding

The released compiler lane configured successfully and built all four
Clipboard targets in **29/29 steps** under the repository's strict warning
policy. The first four-suite run was **3/4**: the repaired codec suite passed,
but Pavel's previously uncompiled atomic-capacity regression expected the
older pinned entry to remain first after admitting a newer unpinned entry. The
model's documented most-recent-first order correctly places that newer item
first, and the same test's later success-path assertions already expect the
pin last. I corrected only the three reversed refusal-path assertions; the
production atomicity behavior was not changed. All four suites are rerunning
now.
