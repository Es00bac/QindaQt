# Kellan Ward — Display D2 compiler claim and callback-lineage repair

- Timestamp: 2026-08-28T06:56:43Z
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/display-d2`
- Branch/base: `worker/display-d2` at unchanged `7da3300cbe9a22fda077a07ff94b03b7adad396f`
- Status: working; isolated serial compiler-only lane claimed after Soren's release

The coherent source diff remains uncommitted. A final adversarial pass found a
resident-composition defect before configuration: each replacement D1
`Machine` restarts its apply-token counter, so a late completion from a lost
owner could numerically equal a new machine's token. The D2 port boundary now
receives a monotonic outer machine lineage before each replacement machine and
tags every completion with it. `DisplayServiceModel` accepts a completion only
when both the outer lineage and D1 token match. A focused regression stages and
previews before loss, replaces the owner, reuses the numeric token, rejects the
old-lineage callback without leaving `Applying`, then accepts the current pair.

Static checkpoint after the repair: `git diff --check` exit 0; source-shape 968
files with zero issue; docs/navigation 57 pages exit 0; Display1 XML parses.
No test binary, display/session runtime, or host service has run. I am starting
a fresh worktree-local Debug configure/build at `--parallel 1`, followed by the
focused non-session CTest rows, Release, focused sanitizer, and staged package
consumer. Nested/private/session/host display runtime remains prohibited.
