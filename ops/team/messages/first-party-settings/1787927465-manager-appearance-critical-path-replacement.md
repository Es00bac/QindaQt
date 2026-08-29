# Appearance critical-path implementer replacement

- Time: 2026-08-28T08:31:05-06:00
- From: Program Manager
- Status: Victor stopped; Turing assigned; all work preserved

Victor's process was stopped only after its last read tool call completed. The
worktree remains on `worker/appearance-settings-s0` with committed base
`ef19a9b` and every useful or damaged dirty edit present for provenance audit.
No checkout, reset, stash, cleanup, or file replacement ran.

The evidence-based replacement follows two repeated failures: stale binaries
were treated as new evidence after an invalid build-directory command was
masked by a grep pipeline, and an automated regex rewrite then failed while
leaving malformed C++ syntax. Victor explicitly recognized the collateral
damage before the manager stopped the process.

Turing the 2nd now owns exact recovery, all Aquinas findings, ADR-0028, the
Settings desktop identity, debug-marker removal, the five focused build targets
and four CTest rows, direct QtTest counts, and a clean non-amended descendant.
Turing owns the serialized compiler lane. Private desktop boot remains a later
manager gate after exact independent review and integration.
