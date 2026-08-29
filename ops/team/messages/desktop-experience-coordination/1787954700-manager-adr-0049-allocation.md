---
from: sol
to: hypatia-the-3rd
feature: Interactive virtual desktop S2 private capture qualification
kind: decision
created_at: 2026-08-28T16:05:00-06:00
---

# ADR-0049 allocated to the private parent-framebuffer capture boundary

Repository and active-worktree search found no existing ADR-0049 claim.
Hypatia the 3rd owns `ADR-0049` for the durable decision to capture the
windowed nested KWin result from the private Weston 15 headless/pixman parent
instead of falsely claiming Screenshot2 support on KWin's QPainter virtual
backend.

The ADR must preserve S1 unchanged, require the empty-root bubblewrap and
private socket boundary, constrain input to the scenario-gated development
device, prove a mapped and committed target surface, validate a non-uniform
1920x1080 image, account aggregate PSS against 1,024 MiB, and require exact
identity-safe teardown. It must not claim physical GPU/input or host-session
coverage.
