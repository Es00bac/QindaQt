# Rhea Calder — virtual desktop observed-identity repair midpoint

- **Timestamp:** 2026-08-28T13:05:45Z
- **Worker:** Rhea Calder (OpenAI Codex `gpt-5.6-sol`, reasoning high)
- **State:** working, source/static-only; no compiler or private-runtime ownership
- **Exact unchanged HEAD/parent:** `e325f2f1e8d69d2d6e3eaa42c04df0f71d2265c7` (tree `ca722256cd0dbd353ae264a571ce6d5e2171168b`)
- **Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/virtual-desktop-s0-s1`

## Implemented midpoint

Ten owned paths now make the installed Settings application's observed
`qindaqt-settings` ID authoritative. They derive one canonical
`Virtual-<zero-based decimal index>` from the exact one-item Outputs inventory,
require ShellVisibility to carry the same identity/geometry/scale and equal
nonzero generation, then bind all consumed production dock current/desired
references to that derived identity. Exact `(0,0)`, 1920x1080, scale 1,
one-output, combined-input, dock PID, service/process, application, PSS, and
cleanup requirements remain unchanged.

The output policy is decomposed into new focused
`tests/session/desktop_session_output.py` and
`tests/session/test_desktop_session_output_unit.py`. Hostile cases accept both
observed `Virtual-0` and a runtime-assigned canonical `Virtual-17`, while
rejecting empty/nonvirtual/negative/leading-zero/wrong-type identities,
Outputs/ShellVisibility drift, visibility geometry drift, dock target drift,
and the launcher metadata ID `org.qindaqt.Settings`. The static CMake syntax
registry now includes the output module/tests and the earlier readiness
module/tests.

## Evidence and boundary

- `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests/session -p 'test_desktop_session_*_unit.py'`: **PASS 54/54**.
- `tools/validate-docs`: **PASS, 64 documents/navigation**.
- `tools/check-source-shape`: **PASS, 998 files, zero warnings/issues**.
- `git diff --check`: **PASS**.

Elara's live analysis still has only claim `1787921694`; its completed handoff
will be consumed before the descendant commit. Devika owns the compiler lane.
No configure, build, CTest/package row, nested/private compositor, session, UI,
or host display/input/cursor/bus/config/hardware endpoint has run.

## Next action

Read Elara's completed analysis, perform final exact diff/schema/static audit,
commit a clean non-amended descendant, and request an independent exact review.
