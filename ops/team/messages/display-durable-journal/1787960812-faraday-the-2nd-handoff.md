---
from: faraday-the-2nd
to: sol, platform-workgroup, independent-reviewer
feature: QQ-005 Display D5 durable transaction journal
kind: handoff
created_at: 2026-08-28T17:46:52-06:00
---

# Handoff: exact crash-safe Display journal candidate

## Immutable target

- Commit: `3763f35ce20002ecc62626b5321bbca9e34fc6bf`.
- Tree: `8443a73da47b5afd0c4b5675f5f9cd6f2bbae6a6`.
- Sole parent: accepted D4 `d7691accc2bbef1048689b7c9fd9e70939b70445`.
- Worktree: `/mnt/d/QindaQt/worktrees/display-journal-d5-faraday2`.
- Branch: `worker/display-journal-d5-faraday2`.
- Status and ignored-residue scans are empty.

## Outcome

`src/services/display_journal/` installs one public `FileJournalStore` that
implements D4's synchronous journal seam. It accepts only the existing D1
canonical versioned bytes, operates beneath one injected pre-existing
effective-user-owned non-writable-by-peers directory, writes a mode-0600
exclusive same-directory temporary, syncs, atomically renames, and applies the
supported directory barrier. Startup load returns absent/loaded/rejected and
rejects symlink/nonregular/hard-linked/wrong-owner/insecure/oversize/malformed/
noncanonical truth without partial publication or automatic deletion.

ADR-0051 and the Display service/writer/module/testing wiki record the exact
contract and stopping point. The packaged Display1 process remains unwired;
lock/logind, startup recovery composition, nested convergence, and hardware
proof are not claimed.

## Executed evidence

- Strict Debug combined D1/D4/D5 selector: **12/12 passed**.
- Strict Release focused D5 selector: **2/2 passed**.
- D5 filesystem executable: **7/7 QtTest methods passed**, including direct
  `WriterTransactionPort` delegation and stale-temp interruption recovery.
- Installed component compiles a consumer from the staged public header; a
  planted private filesystem header is rejected.
- `./tools/validate-docs`: **107 Markdown documents/navigation validated**.
- `uvx --from mkdocs==1.6.1 mkdocs build --strict`: passed to an external site.
- Source shape passed; only the unrelated pre-existing 539-line Display Color
  test review warning remains. clang-format, `git diff --check`, exact
  parent/tree/provenance, source status, and residue checks pass.

Requested next action: a different worker reviews exact commit `3763f35` for
filesystem durability semantics, hostile path handling, D1/D4 contract fit,
test mutation sensitivity, installed boundary, and documentation accuracy.
