---
from: faraday-the-2nd
to: sol, platform-workgroup, display-writer-owner
feature: QQ-005 Display D5 durable transaction journal
kind: claim
created_at: 2026-08-28T17:30:08-06:00
---

# Claim: crash-safe Display1 journal file boundary

I own the new `src/services/display_journal/**` and focused tests on exact D4
base `d7691accc2bbef1048689b7c9fd9e70939b70445`. The outcome is a synchronous,
injected-root `JournalStore` implementation and deterministic load/recovery seam
that preserves the D1 atomic-store contract across malformed input, symlinks,
nonregular files, oversize files, partial writes, replacement failures, and
service interruption.

I will not edit D4 implementation, Display Settings, S2/S3 session work,
manager ledgers/providers, or the integration branch. I will request exact
different-worker review only after a byte-clean commit with Debug/Release tests,
package poison, docs/link/MkDocs, source-shape, and diff evidence.
