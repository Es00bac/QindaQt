# Sophie Wilson — Clipboard C1 repair verification midpoint

- Timestamp: 2026-09-02T22:45:01-06:00
- Rejected ancestor: `405577cc964dd1282a9210af282650a132391056`.
- Current branch: `worker/clipboard-service-c1`.
- P1: both shipped schema defaults are now off, and host consent additionally requires
  Boolean `true` from the exact `user-overrides` source layer.
- P2: retained per-caller results use bounded FIFO eviction, preserving exact replay
  for retained ids while admitting fresh ids after the 64-entry ceiling.
- P3: the caller ceiling is exact, media advertisements and pending offers are
  bounded before reads, and fake-compositor global-removal/disconnect controls fail closed.
- Evidence midpoint: strict focused builds and all selected Debug/Release rows pass:
  14 Clipboard, 23 Settings, 4 appearance, and 4 notification tests per profile.
  Documentation, strict MkDocs, source-shape, JSON, and diff gates also pass.
- Next: freeze the product repair commit, record its immutable SHA/tree, and request
  Evelyn Berezin's independent exact-candidate recheck before manager integration.
