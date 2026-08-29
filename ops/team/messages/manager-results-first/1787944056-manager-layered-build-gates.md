# Manager decision — layer build gates instead of duplicating full builds

- Time: 2026-08-28T19:07:36Z
- Owner: Program Manager
- Scope: every QindaQt implementation, exact-review, and integration lane

Host observation reached 50 concurrent `cc1plus` processes and load 61 on 24
logical CPUs because several module reviewers independently invoked the
default whole-repository build while Vera Kline was already running the one
required combined integration build. This multiplied unrelated compilation
without increasing product confidence.

Effective immediately:

1. An implementer or exact reviewer builds the owned module libraries,
   focused tests, installed consumer/package target, and only adjacent targets
   justified by a changed dependency or a reproduced defect.
2. A module lane must not invoke a default whole-tree build merely to discover
   compile errors. Explicit target names and focused CTest selectors are the
   normal gate.
3. The Program Manager runs one strict combined-tree build and broad safe test
   pass for each accepted integration batch. That is the cross-module proof;
   reviewers do not duplicate it.
4. A tiny repair may reuse its independently reviewed parent's broad evidence
   after the changed behavior, focused tests, adjacent negative controls,
   provenance, docs, and cleanliness are rerun. The exact verdict records why
   the reduced gate is proportional.
5. Runtime/session/hardware-like gates remain serialized. Source review,
   focused tests, documentation, and unrelated implementation continue in
   parallel.

The manager stopped only redundant generated-build processes for Power,
Bluetooth, and Network. No product source, commit, dirty work, transcript, or
candidate was discarded. Vera's combined integration build continues.

