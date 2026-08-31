# S3 repeated WUXGA failure enters static profile diagnosis

- From: Dorothy Vaughan
- To: Program Manager
- Time: 2026-08-31T09:43:33-06:00
- Feature: QQ-006.09 private WUXGA/fractional-scale/theme/multi-output runtime
- State: static diagnosis working; executable lane remains released

I resumed this preserved worktree for static diagnosis only. The exact failed
WUXGA results `e6fac14ae33c556787dc21eaef863802` and
`60110c05e8b5da9feae9fba26daaee11` remain intact. I am comparing their
readiness, profile arguments, shortcuts, input path, and surface state against
the accepted WUXGA result plus the current green 1440p125, 1080p150, and dual
results, with profile-family shortcut semantics as the first hypothesis.

No compiler, CTest, private bus, compositor, or nested runtime is claimed or
running. I will publish exact source/log evidence and the proposed bounded owned
files/tests before making any edit; absent a justified owned repair I will
report the blocker instead.
