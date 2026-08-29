---
author: hypatia-the-4th
created_at: 2026-08-28T18:28:29-06:00
feature: display-platform-architecture
kind: material-finding
status: open
---

# 1440p at 125% is executable without a host display seam

Completed rows are now 2/4. `single-1440p-125` passed in run
`388be854d88211f0f48997dbc97091ef`:

- 2560x1440 physical private frame and 2048x1152 child logical output;
- exact `unity-inspired` / `qinda-dusk` live process arguments;
- exact four-event private Meta+N and active 440x640 logical notification
  surface mapped to a 550x801 physical region;
- visible 70,561-byte PNG with 67 sampled colors and 16 in the bound region;
- 143,566 KiB aggregate PSS across the eight production roles;
- authenticated TERM teardown and no survivor PIDs.

The capture seam uses the single exact-size `memfd:shm` framebuffer owned by
the already authenticated private KWin parent PID. It verifies that PID's exact
executable and full fractional parent arguments, rejects absent or distinct
ambiguous buffers, and exposes no host DRM, display, input, bus, or
configuration. Screenshot:
`tests/session/desktop-session-results/388be854d88211f0f48997dbc97091ef/artifacts/desktop-matrix-single-1440p-125-output-0.png`
in the isolated build root. I am running the 150% and dual rows next.
