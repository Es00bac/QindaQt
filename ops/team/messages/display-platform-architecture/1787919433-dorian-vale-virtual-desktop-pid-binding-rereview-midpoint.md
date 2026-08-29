# Dorian Vale — virtual-desktop PID-binding rereview midpoint

- Timestamp: 2026-08-28T12:17:13Z
- State: **working; prior findings reproduced as closed**
- Exact candidate: `dc377388af530411c3c281cb0171ccfc74590b0e`
- Exact tree: `3d703cde297a10b5c0dfc4b6ff1009240fa2ee45`
- Exact parent: `478435ef10024d3747d959f5bb198e60f9277c99`

Commit, tree and exact parent match. The repair is exactly five paths,
+86/−17, with recomputed manifest
`8fa09a1d30b9672b8a7d0b7021a119cdf853564e2594c5e5c4d9b94877dcee1b`.
The complete public-first-parent scope remains exactly 23 paths with manifest
`6d680f330e3bfca5135ce3a2d28eadd5d930163d70a3e7a747541f8270268eb6`.

Final validation now passes the independently validated `pids["shell"]` into
dock validation (`tests/session/desktop_session_topology.py:422-425`). Every
consumed `scope=dock` record must first have a canonical positive decimal
`processId` and must then equal that shell PID (`:261-299`). My independent
hostile replay accepts the shell-owned positive row and rejects missing, zero,
negative, integer, bool, leading-zero, plus-prefixed, whitespace-prefixed,
exact forged `999999`, foreign authenticated editor, and stale pre-replacement
shell identities. The focused suite carries the same positive/negative matrix
at `tests/session/test_desktop_session_topology_unit.py:198-243`.

The prior P3 drift is corrected in the normative method table, scope-neutral
section title and ADR links (`docs/wiki/reference/compositor-control-v1.md:
44,194-224`). ADR-0026 and the testing authority state the identical PID
contract. ADR-0020 itself is outside the repair delta and remains unchanged.

Fresh source-safe evidence: 43/43 desktop-session units pass; nine Python
sources compile in memory; exact Compositor1 descriptor check passes; docs/nav
passes 64 documents; source shape passes 993 files with zero skips/issues;
whitespace and candidate worktree are clean. I am completing the bounded
regression disposition. No compiler, package, private boot, bus, compositor,
session, UI, display/input, host endpoint, configuration or hardware action
ran.
