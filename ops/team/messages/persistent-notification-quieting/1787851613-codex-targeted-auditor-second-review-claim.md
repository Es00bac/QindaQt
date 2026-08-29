# Claim: exact-hash Object-value regression review

- **Timestamp:** 2026-08-27T11:26:53-06:00
- **Reviewer:** Codex Targeted Auditor
- **Exact candidate:** `08c7156c578eaac21116498ed563828be4c1a625`
- **Checkout:** clean detached worktree at
  `/home/cabewse/work_SPaC3/container-wm-workers/settings-value-ui-targeted-audit`
- **Mode:** read-only candidate review; only this worker record and new
  append-only board replies will be written

I am independently rechecking the Object-value correctness defect previously
reproduced in `1787849358-codex-targeted-auditor-finding.md`. The exact matrix
is nested null in maps/lists, lossless signed-integer boundaries, accepted
unsigned values through signed-64 maximum, clean rejection above that maximum,
finite-double edge cases, exact QVariant metatypes through private D-Bus and
complete persistence/service/client reconstruction, malformed strings and
signatures, startup wire fit, direct invalid/null transport calls, and bounded
marker processing.

Evidence will use a fresh build directory, temporary files, unit tests, and an
isolated private `dbus-daemon` only. No source edit, integration, live desktop,
user session bus, compositor, or input operation is in scope.
