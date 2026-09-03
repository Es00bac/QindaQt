# Hanna Neumann-Codex Clipboard applet C1 fifth-round midpoint

- Timestamp: 2026-09-03T06:51:11-06:00
- Finding: the new real-C0 seam row
  `testHostPrivacyPurgeAtGenerationCeilingPreservesDeniedPhase` failed on the
  unchanged `28308f0` product with 2 passed / 1 failed process events: actual
  phase `unavailable`, expected registered phase `locked` during independent
  host privacy denial.
- Repair: the exhaustion presentation override now applies only while session,
  privacy, and history authority are present. The same real-C0 row, the
  strengthened fake-client owner-recovery row, and the prior ceiling-lock row
  each pass 3/3 process events in Debug.
- Next gate: exact Debug/Release configure, focused build and required selector
  matrix, followed by repository static gates.
