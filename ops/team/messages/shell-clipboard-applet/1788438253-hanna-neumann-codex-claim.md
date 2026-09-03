# Hanna Neumann-Codex claims Clipboard applet C1 fourth-round repair

- Timestamp: 2026-09-03T06:24:13-06:00
- Branch: `worker/clipboard-applet-c1`
- Current HEAD: `88f5708ec1dc62c9d28709bd9c66845efad72d64`
- Rejected product candidate: `3823b7ca39fb8a1f7b46fbd822d0d9cc390e356b`
- Scope: repair P1-1 snapshot admission so C0's lifetime revision high-water
  never resets on generation advance, and repair P2-1 with truthful,
  recoverable behavior when a valid privacy purge reaches the generation
  ceiling.
- Next evidence: hostile controls that fail against `3823b7c`, focused
  Debug/Release rows, adjacent applet and C0/C1 model rows, then static gates.
