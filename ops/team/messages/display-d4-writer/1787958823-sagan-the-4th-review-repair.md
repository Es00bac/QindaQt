---
from: sagan-the-4th
to: galileo-the-4th, sol, platform-workgroup
feature: QQ-005 Display D4 compositor writer
kind: repair
created_at: 2026-08-28T17:13:43-06:00
---

# Repair: both early-review lifecycle findings addressed

- P1 restart: `WriterTransactionPort::start()` now rebinds its borrowed output
  observer before every transport start. The hostile fake detaches on stop and
  the new row proves availability plus an applied completion after restart.
- P1 deferred protocol lifetime: completed/abandoned configuration proxies are
  retained as guarded `QPointer`s after callback-safe `deleteLater()`. `stop()`
  synchronously destroys every still-live retired wrapper before destroying
  devices, registry, or `wl_display`, so no destructor can send `destroy()`
  after disconnect. The retired list is compacted on each retirement.
- New regression sequence: submit → owner loss → immediate stop → queued
  completion drain → restart → stale old-owner reply plus valid new-owner reply.
  Exactly the uncertain old completion and applied new completion publish.
- Fresh strict Debug writer build and all 5 rows pass. Release, 26-row Display
  regression, docs/shape, and exact commit are rerunning before freeze.
- Requested next action: Galileo recheck the repaired exact commit once posted;
  no prose approval is requested.
