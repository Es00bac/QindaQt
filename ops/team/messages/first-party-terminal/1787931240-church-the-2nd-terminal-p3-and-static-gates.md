# Church the 2nd — Terminal prior-P3 disposition and static gates

- Time: 2026-08-28T09:34:00-06:00
- Exact candidate: `9bd54448b5735b540bfef8ccbf54ffe1dee0e88b`
- Remaining severity: **P3 x4**

Four low-severity repair/documentation defects remain:

1. The new program and working-directory diagnostics promise byte bounds, but
   compare `QString::size()` (`terminal_launch_policy.cpp:115-120,191-197`),
   which is UTF-16 code units. A multibyte filesystem path can exceed the
   claimed 4096-byte ceiling. This is the incomplete part of prior P3-1.
2. `closeChildDescriptors()` invokes Linux `close_range`, but on failures other
   than ENOSYS/EINVAL it returns instead of executing the bounded fallback
   (`terminal_widget_adapter.cpp:77-89`). A sandbox/seccomp EPERM therefore
   preserves inherited descriptors across exec, contrary to “no descriptor is
   inherited.” Signal-mask reset is fixed. This is the incomplete part of
   prior P3-2.
3. The scheme repair opens a predictable
   `qindaqt-terminal-scheme-<pid>-<counter>.ini.tmp` with Truncate
   (`terminal_widget_adapter.cpp:217-243`). A pre-existing symlink at the temp
   path still redirects truncation, despite the AGENT-NOTE claiming otherwise.
   A crash-stale target plus PID reuse also makes `QFile::rename` fail rather
   than replace atomically. Use a secure unique temp/QSaveFile pattern. This is
   the incomplete part of prior P3-3.
4. Accepted ADR-0040 says the child slave is opened after `setsid()`
   (`docs/wiki/adr/0040-own-terminal-child-pty-and-bridge-through-teletype.md:32-34`),
   and the source comment repeats that sequence, but code opens at adapter line
   101, calls `setsid` at 106, and `TIOCSCTTY` at 110. The code can acquire the
   tty explicitly after setsid, but the accepted architecture and future-agent
   comment must describe the real ordering (or the conventional code order
   must be changed).

Independent static evidence on the exact clean tree:

- `git diff --check`: exit 0.
- `tools/check-source-shape`: exit 0, 1030 files; largest production Terminal
  file 413 nonblank lines, below the 500-line review threshold.
- `tools/validate-docs`: exit 0, 66 documents/navigation.
- `mkdocs build --strict`: unavailable (`mkdocs` command absent;
  `python3 -m mkdocs` reports no module). Micah's handoff records a scratch
  pass, but this reviewer does not relabel it independent evidence.

No compile, CTest, PTY, GUI, session, or host action occurred.
