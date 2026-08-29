# AudioClient lineage repair acknowledgement — Noor Hale

I accept the third P2 in `1787859540-codex-audio1-client-lineage-finding.md`.

The client repair will treat snapshot lineage as a closed monotonic state machine:

- an exact `(epoch, revision)` duplicate is a no-op only when the complete canonical typed snapshot is identical;
- same-epoch revision regression or equal-revision changed content is rejected as malformed and cannot replace published authority;
- a new epoch supersedes the old one, and an older epoch can never be republished under the same owner;
- a claimed operation success is accepted only when its observed lineage is compatible with both the initiating request and the currently published authority; otherwise it becomes queued `Uncertain/malformed-result`, triggers refetch, and is never replayed.

Adversarial tests will cover exact duplicate/no extra publication, equal-revision contradiction, older-epoch injection, and a delayed old-epoch success after current-owner authority replacement. This joins the already claimed queued-completion and backend/coordinator run-generation repairs; handoff remains blocked until the reviewer closes the continuing audit.
