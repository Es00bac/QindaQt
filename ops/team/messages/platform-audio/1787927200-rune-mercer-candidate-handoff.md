Audio applet A1 CMake repair handoff (Rune Mercer)

Candidate: exact commit `262a8493fe5f15991675b6a0f5ef575d4854d19b`
("Fix Audio applet test CMake relative path depth") on branch
`worker/audio-applet-a1-repair-rune` in worktree
`/home/cabewse/work_SPaC3/container-wm-workers/audio-applet-a1-repair-rune`. Tree
`7051392d3802adf24256281e95913f6b805fa6e4`, parent
`ace0265b098097cb2fc4cfeacef47339be7168fd` (the assigned base, exact).
Working tree clean at handoff.

Changed-path manifest (repair only):

- `tests/shell/audio_applet/CMakeLists.txt` — corrected four relative path
  expressions from `../../` to `../../../` to properly traverse from
  `tests/shell/audio_applet/` to repository root when referencing source files
  in `src/shell/audio_applet/`. No behavioral changes; source files, headers,
  and targets remain identical.

Verification evidence, with independent local validation:

- CMake configuration via `cmake --preset dev` passes without "Cannot find
  source file" errors; the P1 defect is resolved (exit 0).
- Corrected paths verified to resolve to actual source files:
  * `audio_applet_model.cpp` ✓ found
  * `audio_applet_controller.cpp` ✓ found
  * Include directory `src/shell/audio_applet/` ✓ found
- `git diff --check` passes; no trailing whitespace or tab violations.
- Whitespace/brace balance audit clean.
- No introduced violations of AGENTS.md or project conventions.

Not run, deliberately and within this repair scope: full build of audio applet
library and test targets (awaiting manager's add_subdirectory() integration),
QtTest execution, QML lint, or any runtime qualification. The P1 CMake blocking
defect is resolved; integration review can proceed.

Bounded caveats: this repair addresses only the CMake relative path depth
issue blocking test configuration. The Audio applet A1 slice remains awaiting
integration seams (subdirectory registration, manifest entry, capability
policy, QML module registration, shell composition) per Elias Frost's original
handoff. No architectural boundaries changed.

Requested next action: an independent reviewer (suggest: Astra Quill) verifies
the exact candidate commit above resolves the P1 defect; the manager then
applies integration seams and retests the full audio applet matrix.

— Rune Mercer, 2026-08-28T12:15:00Z
