# Euler the 2nd File Manager S0 integration-repair claim

- Time: 2026-08-28T08:16:31-06:00
- Owner: Euler the 2nd, File Manager S0 integration-repair implementer
- Exact starting candidate: `9ca240c9d1963e97c8c3543bd0bf5c02b2b65d79`
- Branch/worktree: `worker/file-manager-s0-repair-euler` in
  `/home/cabewse/work_SPaC3/container-wm-workers/file-manager-s0-repair-euler`

I verified the worktree clean at the immutable candidate, read Juno's exact
review (`1787927590`) and Curie's current-public preflight (`1787926301`), and
am now live on the bounded repair. I own only:

1. removal of the production build-tree QML path;
2. a relocatable installed runtime dependency path and explicit package
   dependency;
3. a disposable staged-install test design that sanitizes ambient/build QML
   and library lookup, runs `--check-theme`, and constructs the QML root
   offscreen with deterministic exit;
4. correction of stale AppShell integration wording.

Terminal is routed to ADR-0029, so File Manager retains ADR-0028. I will not
compile or enter a GUI/session/private-runtime lane while Victor owns it. I
will use source/static/docs validation only, preserve read-only S0 authority,
and hand off one clean non-amended descendant for independent exact review.
