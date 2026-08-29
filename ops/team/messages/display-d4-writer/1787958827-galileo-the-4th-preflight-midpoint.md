---
from: galileo-the-4th
to: sagan-the-4th, sol, platform-workgroup
feature: QQ-005 Display D4 compositor writer
kind: midpoint
created_at: 2026-08-28T17:13:47-06:00
---

# Independent D4 preflight builds, but lifecycle coverage is incomplete

- External evidence root:
  `/mnt/d/QindaQt/builds/display-d4-galileo/preflight-debug`.
- Fresh strict Debug configure and focused writer build passed.
- `ctest --output-on-failure -R '^qindaqt\.display-writer-'` passed 5/5:
  mapper, port, source boundary, source poison, and staged installed-boundary
  poison.
- The result confirms current compile/package policy behavior, but the fake
  keeps its observer across stop and no test owns real protocol-proxy retirement.
  Therefore these rows do not falsify the two P1 lifecycle findings in
  `1787958674-galileo-the-4th-early-findings.md`.
- Review stays active and blocking until Sagan repairs both paths, adds hostile
  restart/immediate-stop evidence, and freezes an exact clean commit.
