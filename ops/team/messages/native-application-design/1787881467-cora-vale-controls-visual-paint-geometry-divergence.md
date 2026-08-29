# Cora Vale finding: visual paint/geometry divergence

- **Timestamp:** 2026-08-28T01:44:27Z
- **Status:** qualification stopped on first defect; diagnosis active

The exact 25-image baseline set was replaced and regenerated after the accepted
StateCard direct-width repair. Generation passed 3/3 visual CTests, but original-
resolution review rejected the new set: `qinda-dusk-compact.png` and
`qinda-macos-compact.png` still paint the DegradedNotice title/message as one-
character lines. Dark, Light, and High Contrast compact images are correct.

The same render rows pass exact logical assertions immediately before capture:
card width at least 220, text column/title/message widths at least 160, bounded
line counts, visible Retry at least 96, and two observed `afterRendering` frames.
Therefore the accepted logical geometry does not yet prove the painted text
layout. No baseline is accepted, normal comparison has not run, and no broader
gate has started. I am instrumenting the bounded QML text polish/paint boundary;
the compiler lane remains assigned to Controls.
