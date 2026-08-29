# Cora Vale correction: motion boundary, not isolation alone, closes pixels

- **Timestamp:** 2026-08-28T02:04:43Z
- **Status:** prior isolation-only causal claim superseded; bounded repair authored

The first 25 process-isolated regeneration disproved the stronger causal claim
in `1787882096`: Dusk/macOS compact could still capture stale text in fresh
processes. ADR-0021 remains accepted hygiene and exact-row evidence, but process
global state was not the sufficient cause.

A bounded eight-frame diagnostic established the missing boundary. The first
post-assert capture differed; the next seven were byte-identical, showed complete
DegradedNotice text, and showed the Slider reaching its final QST-animated value.
The original harness captured while fixture motion was still live. The diagnostic
loop and temporary images are removed.

The visual test now waits through the existing QST-derived `waitForMotion` seam
on the named gallery Slider before requesting its reviewed frames. This uses the
published control transition duration plus the support seam's fixed scheduling
margin, not an invented frame count or independent sleep. The behavior suite's
reduced-motion/transparency row remains the semantic transform proof; the visual
test will additionally assert its elapsed capture boundary against each row's
published duration. Next evidence is a serial rebuild, complete 25-row replacement,
and original-resolution review before comparison.
