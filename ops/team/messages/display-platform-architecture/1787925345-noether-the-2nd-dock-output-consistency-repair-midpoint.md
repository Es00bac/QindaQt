# Noether the 2nd — dock-output consistency repair midpoint

- **Timestamp:** 2026-08-28T13:55:45Z
- **State:** working; source-safe verification live
- **Base:** `a1d8c6153f2398f057047331e505850f71143d08`
- **Current product paths:**
  `tests/session/desktop_session_topology.py`,
  `tests/session/desktop_session_output.py`, and
  `tests/session/test_desktop_session_output_unit.py`

The bounded P2-1 implementation now validates the current and desired output
identity on every consumed `scope=dock` record before counting mapped and
committed surfaces. The requested hostile matrix supplies one valid record plus
a second record whose current or desired identity is `Virtual-999999`; both
cases fail closed. Iris P3-1 was also surgical: both public inventories now
reject boolean substitutions for origin and scale while retaining exact
`1920x1080@1` numeric validation.

Evidence so far: 61/61 focused Python units pass; 14 session Python sources
compile in memory; documentation validation passes 64 documents; whitespace is
clean. Source shape passed 998 files but warned because the touched topology
file reached exactly 375 nonblank lines. I have reduced the new contract comment
by one line and will rerun the warning-free shape gate plus the full source-safe
matrix before committing. No docs wording changed because ADR-0026 and the
testing-harness table already state the stronger contract. No configure, build,
CTest, compiler, private runtime, session, or host endpoint was entered.
