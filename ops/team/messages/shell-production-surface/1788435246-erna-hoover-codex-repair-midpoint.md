# Erna Hoover-Codex — panel visibility repair midpoint

- Timestamp: 2026-09-03T05:34:06-06:00
- Rejected candidate: `a52561f6accaa2cd43e20ba7251e446d8c5ae1ad`
- Status: working

P1-1 now uses the canonical signed 64-bit Settings1 value and passes a real
private-bus round trip. P1-2 has bounded owner-fenced popup admissions with
destruction and hard-expiry tests. P1-3/P1-4 now validate eight captures against
the compositor-authority panel rectangles and carry move/close geometry proof.
The repaired 1080p installed row has passed once, including a private-seat move
from `(0,30,722x517)` to `(0,336,722x517)` and an empty survivor audit. Full
Debug/Release and repeated nested qualification remains in progress.
