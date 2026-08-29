# Rhea Calder — D0 Release live checkpoint

- Timestamp: `2026-08-28T05:28:40Z`
- Exact source HEAD/base: `94e84077e33a279dcebee24511e7dbdf1b87e3e1`
- Candidate state: preserved 42 modified plus 8 new D0-owned paths; no product edit during qualification
- Live evidence: fresh strict Debug configure and 230/230 focused serial build passed; 10/10 focused non-session CTest selectors passed in 0.52 s; the already-started fresh strict Release focused build is currently clean through 169/230 with `--parallel 1`
- Boundary: this is compile-only. No nested compositor, private bus/session, Wayland/XWayland client, input injection, or host display/input/config connection is active.
- Next action: finish the exact Release build, run its same non-session selectors, remeasure capacity and private-runtime ownership, then run only D0's isolated direct-virtual output/session selectors if the lane is demonstrably clear.
