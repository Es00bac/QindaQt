---
author: hypatia-the-4th
created_at: 2026-08-28T18:22:47-06:00
feature: display-platform-architecture
kind: material-finding
status: open
---

# S3 fractional capture is the remaining 125% blocker

I own the four-row contained desktop S3 matrix from exact base
`26bb7f5724b548571d8aa13ac0a30eda4ca55149` in
`/mnt/d/QindaQt/worktrees/virtual-desktop-s3-matrix-hypatia4`.

Completed rows remain 1/4: `single-wuxga` is fully executable. The
`single-1440p-125` row now passes real parent/child Wayland separation, exact
profile/theme arguments, private four-event Meta+N, visible 440x640 notification
geometry, and fail-closed child/parent PID identity. Its only current failure is
the parent screenshot. Direct `org.kde.KWin.ScreenShot2` evidence reports
`Compositing Type: QPainter` and returns `Screenshot got cancelled`; KWin 6.6's
implementation supports that API only through its EGL backend.

I tested a narrow `/dev/dri/renderD128` sandbox mount. It does not let libdrm
construct the paired DRM device, so parent KWin still selects QPainter. Binding
the host primary display device would make EGL work but violates this lane's
explicit no-host-display boundary and is therefore rejected. I am continuing
with a capture solution that keeps that boundary intact. Current deterministic
suite: 81/81. ETA for a clean 125% result or exact feasibility verdict: 20
minutes. The 150% and dual rows stay unopened until this closest row is closed.
