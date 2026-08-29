# Octavia Snow — Power P1 stale-marker exact evidence and repair route

- Time: 2026-08-28T12:42:53-06:00
- Held candidate: `d11a69d36c30d5100c3878fd0fa505c792ad1c6b`
- Implementer route: Sela North profile
  `ops/team/workers/sela-north.md`, branch
  `worker/power-applet-p1-repair-sela`, worktree
  `/home/cabewse/work_SPaC3/container-wm-workers/power-applet-p1-repair-sela`
- Recoverable Gemini conversation: exact durable timeout record
  `1787937530-mira-tan-sela-timeout-preservation.md` names
  `f91cdc70-daea-4a83-8e87-72de6092c83f`; the subsequent
  `1787937793-mira-tan-sela-resume-routing.md` states that exact conversation
  resumed. No different Sela conversation identifier is present in the board.

Exact stale-marker evidence on candidate bytes:

- `src/shell/power_applet/CMakeLists.txt:3-9` says the directory "is not yet
  wired into the build" and still requires the exact parent registration that
  `d11a69d` adds to `src/CMakeLists.txt`.
- `tests/shell/power_applet/CMakeLists.txt:3-5` says it is "not yet wired into
  tests/CMakeLists.txt" and still requires the exact registration that
  `d11a69d` adds there.

Both are searchable `AGENT-NOTE` markers. Root AGENTS.md says a stale marker is
a defect and must be updated or removed when the associated constraint changes.
This is why Corin Vale's labelled PASS `0/0/1/0` is held rather than milestone-
committed.

Resume Sela through the exact profile/conversation/worktree route above for one
comment-only, non-amended descendant. Preserve all nine functional/documentation
paths and the accepted full-build evidence. Return the exact descendant to
Corin Vale, permanent cross-provider reviewer at
`ops/team/workers/corin-vale.md`, for a detached exact rereview of the two
markers, provenance, focused selector, source shape, docs, and clean state.
