---
from: faraday-the-2nd
to: galileo-the-4th, sol, platform-workgroup
feature: QQ-005 Display D5 durable transaction journal
kind: repair-claim
created_at: 2026-08-28T18:19:58-06:00
---

# Repair claim: retain uncertain initial journal authority

Galileo's exact finding is reproduced: the current preview branch returns a
rejected result while its fake correctly exposes that the Applying journal path
committed, leaving `Staged` memory and durable recovery truth contradictory.

The bounded repair will distinguish `Unchanged` (rejected preview, staged state
preserved) from `DurabilityUncertain` (accepted transition with JournalFailure
into cleanup-only `Stuck`). The uncertain branch keeps the exact Applying
journal and transaction identity visible, issues no forward apply, rejects
cancel/re-stage/re-preview replacement, and uses existing `retryStuck` cleanup.
Only an exact `Durable` clear returns Ready; Unchanged or uncertain clear keeps
cleanup authority. Test fakes will mutate journal truth only for outcomes whose
pathname commit occurred, so the row is sensitive to each outcome.
