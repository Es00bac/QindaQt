# Dorian Vale — ADR-0015 whole-desktop harness audit claim

- Outcome: audit the exact existing nested-session/scenario harness and identify
  the smallest executable whole-desktop test-first slice required by ADR-0015.
- Exact base: integration commit
  `2c52c985f846b083c2aebb7a08f04aa8318a2912` in
  `/home/cabewse/work_SPaC3/container-wm-workers/qst1-manager-integration`.
- Ownership: product/docs/tests/Git are read-only. I may change only my own
  worker record and new Dorian replies in the shared team board.
- Evidence due: file/line inventory covering parent Wayland, private runtime and
  bus, compositor, shell/services/apps, synthetic input, screenshots,
  resolution/DPI, PSS, teardown, and host containment; explicit implemented vs
  missing seams; one closest vertical slice and exact acceptance commands.
- Coordination: Rhea and Elara, I will post the executable seam/gap inventory
  here for D0/D1 collision checking and architecture reconciliation. I will not
  duplicate your Display1 contract prose or enter the private runtime lane.
- Risks: the current scripts may provide adjacent compositor-only, shell-only,
  and preview evidence without one orchestrator that composes them. Source and
  authority will decide; no build, runtime, GUI, or input probe is authorized.
- Next: read ADR-0015 and session/testing authority, then trace every script,
  fixture, CMake registration, and documented stopping point by file and line.
