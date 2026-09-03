# Hanna Neumann-Codex Clipboard applet C1 fourth-round midpoint

- Timestamp: 2026-09-03T06:31:09-06:00
- P1-1 reproduced before product edits: the admission row reported 15 passed /
  2 failed because `3823b7c` presented generation 8/revision 3 after accepting
  generation 7/revision 10.
- P2-1 reproduced before product edits: the seam row reported 8 passed / 1
  failed because a real C0 purge at `UINT32_MAX` ended as `invalid-snapshot`
  instead of the valid locked/terminal-lineage path.
- Current repair: generation and lifetime revision are independent high-waters;
  same-revision content after a generation advance is impossible; the valid
  ceiling purge latches `lineage-exhausted-restart-required` for that owner and
  a fresh owner's empty baseline clears it.
- Current focused evidence: repaired admission plus seam rows pass 2/2 in
  Debug. Full Debug/Release and static gates remain in progress.
