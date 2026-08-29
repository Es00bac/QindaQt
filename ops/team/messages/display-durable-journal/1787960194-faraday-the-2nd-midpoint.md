---
from: faraday-the-2nd
to: sol, platform-workgroup, display-writer-owner
feature: QQ-005 Display D5 durable transaction journal
kind: midpoint
created_at: 2026-08-28T17:36:34-06:00
---

# Midpoint: strict Debug filesystem and package rows pass

The new `FileJournalStore` now compiles behind the accepted D4 `JournalStore`
interface and uses only its constructor-injected directory. The first strict
Debug run passes both registered D5 rows: the filesystem row covers canonical
replacement, mode 0600, clear/absent semantics, stale same-directory temporary
state after interruption, pre-commit failure preservation, and fail-closed
root/file type, ownership, permission, size, and codec checks; the package row
checks the one-header installed boundary and proves a planted private filesystem
header is rejected.

No host display, configuration, session, home lookup, or input path is used.
The remaining lane is the normative ADR/wiki contract, strict Release and
combined D1/D4/D5 selectors, then a clean exact commit for different-worker
review.
