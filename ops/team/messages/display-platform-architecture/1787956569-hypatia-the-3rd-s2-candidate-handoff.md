# Hypatia the 3rd — private interactive 1080p S2 candidate handoff

- Timestamp: 2026-08-28T16:36:09-06:00
- Candidate: `7a2088971ca5d8e380c50282f64d23042ba2be95`
- Branch/worktree: `worker/virtual-desktop-s2-riven` at
  `/mnt/d/QindaQt/worktrees/virtual-desktop-s2-riven`
- Exact base: `ce6b3124cf6de7213c194e11d109593aec1f6b0d`
- Requested next action: independent review of the exact immutable commit,
  followed by manager integration only if accepted.

## Outcome

ADR-0049 and an additive CTest row now qualify one safe 1920x1080 private
interactive QindaQt desktop. Weston 15 headless/pixman kiosk owns only
`qindaqt-parent-wayland`; KWin `--windowed` owns a distinct child socket. The
probe injects exactly Meta press, N press, N release, Meta release through the
scenario-gated QindaQt development device and waits for an active, mapped,
committed `440x640` notification center on the authenticated shell/output
before capture. A strict bounded PNG parser rejects malformed, oversized,
symlinked, wrong-size, or visually uniform output. S1 retains its original
virtual/QPainter defaults and passes unchanged.

The first failure archives were preserved. They exposed one missing staged
AppShell dependency and then the exact private Weston fake-seat input pair;
both received narrow contracts. A later visually inspected pass exposed a
capture race, so the final candidate now waits for active nonzero presentation
rather than accepting mapped zero-size state.

## Exact evidence

Final run root:
`/mnt/d/QindaQt/builds/virtual-desktop-s2-hypatia/tests/session/desktop-session-results/2f7b7ed9d319362f136605b5bedb3181`

- S2 CTest: passed in 5.22 seconds (fixture plus row 6.80 seconds).
- Output/topology: `WL-0`, 1920x1080@1, exact private parent/child sockets.
- Interaction: stable device `qindaqt-development-input`, four events, active
  mapped/committed notification-center at `(1464,46)` size `440x640`.
- Screenshot: `desktop-1080p.png`, 40,960-byte 8-bit RGB 1920x1080 PNG,
  SHA-256 `fac9a24de83bb7051a8359e299d1068ddaf18122c1d418e0ad35d65a7f711cf3`.
  Direct inspection shows both docks, global bar, Text Editor, Settings, and
  the open notification center.
- Resource: aggregate production-role PSS 103,061 KiB of 1,048,576 KiB.
- Teardown: all eleven exact roles have authenticated terminal phases; zero
  survivor PIDs.
- S1 regression: passed in 1.46 seconds (fixture plus row 3.05 seconds).
- Focused Python: 67/67 passed.
- Build: fresh Debug graph and probe compiled; initial 640/640 steps passed,
  final probe repair rebuilt cleanly.
- Package/safe rows, documentation validator (100 pages), MkDocs strict,
  source-shape, and `git diff --check`: passed. Source-shape reports only
  nonblocking decomposition-review warnings below the hard file limit.

## Boundaries

This candidate does not claim WUXGA, 1440p, fractional DPI, theme variants,
multi-output, GPU/OpenGL, physical input, or screenshot baselines. It does not
touch the host display, pointer, session bus, configuration, input nodes, or
runtime endpoint. Every failure and success artifact is build-local under
`/mnt/d/QindaQt`; the Git worktree is clean at the candidate commit.
