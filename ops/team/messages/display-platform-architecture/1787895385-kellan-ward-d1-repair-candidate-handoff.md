# Display D1 exact repaired candidate handoff

- **Timestamp:** 2026-08-28T05:36:25Z
- **From:** Kellan Ward, Display D1 transaction implementer and lead
- **Exact repaired candidate:** `0a8d0e0eac9e0d7c5932fb54b875667b5d7f1639`
- **Tree:** `63617b3a07620b237a74cf2416191d61cd866d3e`
- **Parent:** `0e38fa726af69e34be3cacdd6b71d40350ac8092`
- **Exact public base of the two-commit series:** `94e84077e33a279dcebee24511e7dbdf1b87e3e1`
- **Subject:** `Repair Display1 projection and recovery contracts`
- **Scope:** exactly 15 owned paths, `245 insertions, 26 deletions`

## Repaired outcome

This non-amended descendant closes Elara Finch's exact-candidate P1 and all
accepted low-cost P2/P3 repairs. Mirror root scale/position projection now
finishes before one global-origin translation, so both output orders produce
the same translated mirrored candidate/fingerprint and rollback converges to
the full translated preimage. Exact regressions pin both orders, no-op truth,
fingerprint truth, and translated-mirror rollback.

The same repair preserves current-lineage routing, settle barriers, total
revert-attempt bounds, cleanup-only journal failure, full-preimage set-flap
rollback, survivor properties, explicit Stuck/terminal reasons, and hostile
codec/value limits. `transaction_types.h` now directly includes the public
`display_limits.h` that owns the shared retry limit, closing Mina Shah's P0
without changing dependency direction. Mina's fresh source/API/docs rereview
`1787891900` is PASS with no remaining P0-P3 finding.

## Exact qualification

All compiler work used fresh worktree-local roots and `--parallel 1`.

| Gate | Exact terminal evidence |
| --- | --- |
| Strict Debug focused | configure exit 0; build 77/77 exit 0; Display CTest 11/11, 0 failed, exit 0, 0.08 s |
| Strict Debug broad | build 749/749 exit 0; pure/static batch 54/54 exit 0, 2.40 s; explicit non-session service/model batch 39/39 exit 0, 5.86 s |
| Strict Release focused | configure exit 0; build 77/77 exit 0; Display CTest 11/11, 0 failed, exit 0, 0.09 s |
| Strict Release broad | build 749/749 exit 0; exact non-session inventory 90/90, 0 failed, exit 0, 7.49 s |
| ASan+UBSan focused | configure exit 0; build 77/77 exit 0; Display CTest 11/11, 0 failed, exit 0, 0.41 s with leak detection and ASan/UBSan halt-on-error |
| Staged install/package | install exit 0; exactly 4 Display static libraries and 15 public Display headers; standalone consumer's first QindaQt include is `transaction_types.h`; configure exit 0, serial build 2/2 exit 0, execution exit 0 |
| Documentation/source | `validate-docs` exit 0 (51 pages + navigation); strict MkDocs exit 0 (0.40 s); source-shape exit 0 (885 files, zero allowlists); forbidden dependency/runtime scan exit 0; `git diff --cached --check` exit 0 |

Two harness-only stops did not execute a test or compiler and are not product
defects: `/usr/bin/time` was unavailable (wrapper exit 127), and CTest rejected
a relative `--tests-from-file` path (exit 8) before its absolute-path retry
passed 39/39. The Debug 54-row batch included three session-named command-line
unit binaries due CTest range renumbering; no session/display/service process
was launched. Release used the corrected explicit 90-row non-session file.

No display, Wayland/XWayland, private D-Bus/session, GUI, input, host service,
hardware, or nested runtime ran. This remains deterministic D1 model/package
evidence; D0/D2/D8 own their later nested/service/hardware boundaries.

## Exact exclusion and next action

Only the 15 owned repair paths were committed. `.omc/` and
`ops/team/workers/kai-mercer.md` remain untracked and excluded.

Requested next actions:

1. Elara Finch rereviews **exactly** commit `0a8d0e0eac9e0d7c5932fb54b875667b5d7f1639`,
   first reproducing the translated `[A,B]` and `[B,A]` mirrored-order and
   rollback traces, then checking the previously reported P2/P3 matrix.
2. On exact acceptance, the manager applies the accepted two-commit series
   (`0e38fa72`, then `0a8d0e0`) atomically onto public main, preserving the
   already-mapped additive integration collisions and rerunning integrated
   gates.

Kellan is waiting/not live and remains available only for an exact reproduced
review defect.
