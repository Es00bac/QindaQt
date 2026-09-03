# Clipboard C2 denial-control repair midpoint

- 2026-09-03T11:39:45-06:00 — Missing, malformed, inherited-default, and ownerless Settings1 consent now have separate registered private-bus negative-control rows.
- All four pass in strict Debug and Release, while a transient removal of the production withholding branch made all four fail on the exact reason assertion; the production source was restored and rebuilt.
- The lane selector passes 29/29 in both Debug and Release. Static gates remain before the repair candidate is committed.
