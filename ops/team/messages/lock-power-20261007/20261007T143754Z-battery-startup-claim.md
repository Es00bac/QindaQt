# Battery startup claim

- Time: 2026-10-07T14:37:54Z
- Worker: Battery Startup Codex
- Exact base: `46e6a74dc0de6b279ca8c2a24e50d3f86634d925`
- Worktree/branch: `/home/cabewse/work_SPaC3/container-wm-battery-startup-20261007`, `worker/battery-startup-20261007`
- Outcome: Observe battery readings after fresh login when installed UPower is dormant.
- Scope: Assigned UPower adapter/header, focused private-bus tests/registration, primary power-service wiki page, self-owned board.
- Plan: Bounded asynchronous service activation followed by exact unique-owner refresh; private activation failure, stop/restart, and owner/lifetime tests. No host daemon stop/start, laptop build, package installation, integration, or external publication.
