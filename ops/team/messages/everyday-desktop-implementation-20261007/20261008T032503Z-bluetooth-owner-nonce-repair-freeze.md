# Bluetooth exact owner/nonce review repair — source recheck requested

- Original immutable source: 762de6ba1af74b4b65796362e4d8021875a6c533.
- Same assigned worktree/branch and base5b7; no rebase.
- Repaired scope: RadioOperation ledger/types, owning policy/private-bus fixtures, staged CMake child builds, ADR0359/owning harness and self-authored records only.

The ledger now retains globally bounded (exact unique owner, nonce) keys until each deadline. An admitted owner switch does not clear entries. Capacity512 refuses new entries instead of evicting unexpired ones. Current owner/caller/full-intent admission is unchanged. New actual-engine fixtures exercise A→B→A replay, equal nonce under different owners, global capacity without eviction, and capacity release only after expiry. A separate fixture relinquishes and reacquires the real well-known alias on a private bus around the production engine, with injected platform observations/writes. This is not a claim of end-to-end physical replay; full-intent and completed-request retirement remain additional barriers.

The installed consumer recipe uses ordinary cmake --build for positive, withheld and restored builds. It no longer hard-codes manager-native -j24/-l24 in portable source. Exact-header withholding/restoration behavior is preserved.

Actual source-only verification: docs531/navigation exit0, strict MkDocs exit0 (16.38s), radio sandbox/public boundary exit0, diff check exit0. These new fixtures are authored, not compiled or run. Original762 remains the old control for a future explicitly leased unchanged-fixture comparison using its matching implementation/header layout. No compiler, private bus, device, namespace, radio or installed action occurred.

Requested next action: Platform rechecks this exact descendant; manager grants native only after source acceptance. Full BT native/RW/installed gates remain open. No additional source scope, package privilege or main-unit changes.
