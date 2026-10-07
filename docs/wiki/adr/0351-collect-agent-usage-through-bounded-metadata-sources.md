# ADR-0351: Collect agent usage through bounded metadata sources

- Status: Proposed
- Date: 2026-10-07
- Deciders: QindaQt owner; independent candidate review pending

## Context

The owner requests a panel plugin beside hardware status controls that shows
remaining coding-agent limits and reset times alongside token and cost totals.
Providers expose different account and session metrics. Missing quotas or costs
cannot be inferred from token counts, and presentation must not become a second
owner of account credentials or agent conversations.

## Decision

Use a small public Qt Core source boundary in services/agent_usage, consumed by
a separate shell projection. Same-thread AgentUsageSource provides typed owning
snapshots and explicit asynchronous refresh. Shell-private composition owns
collector lifetime and permission decisions. No daemon, database, D-Bus endpoint
or new dependency is introduced.

Only the audited builtin may receive agent-usage.read. Denial withholds rows and
performs no collection. Construction performs no I/O. Deliberate popup open or
Refresh triggers one bounded flight with no shell-startup network request.

Codex uses documented stdio app-server account metadata RPCs. No thread/turn,
login, token refresh, credit reset or billing action is requested. Credentials
stay CLI-owned. Multi-bucket limits retain identity/reset windows; unsupported
methods/auth modes remain visibly unavailable.

Strict normalized reports support Claude's documented statusline fields and
other provider publishers. An optional installed stdin publisher stores only
allowlisted metrics in private XDG configuration. It never edits existing agent
hooks or settings. Reports retain provider, observation time, provenance,
tokenScope and costScope: Claude context tokens differ from session cost, while
Codex lifetime tokens have no inferred USD cost.

File/row/process bounds, timeouts, atomic publication and invalid/stale handling
belong to the backend. QML never reads files or executes agents. Unknown, stale
and error data stay distinguishable. No pricing table, scraped page or invented
subscription allowance substitutes for reported metrics.

The builtin manifest, runtime dispatcher and popup lifecycle remain under the
[applet runtime](../shell/applet-runtime.md). Stock profiles place the icon with
their existing hardware group, including profiles whose group is not on top.

## Consequences

Sources retain provenance and scope; providers without a documented metadata
source or normalized publisher need setup and show unavailable data meanwhile.
Offline refresh retains timestamped stale data rather than reporting zero.
Publisher/reader share a persistence contract requiring round-trip, hostile
input, resource-bound and expiry tests. Public same-thread ownership/callbacks
and teardown remain explicit. Compiled popup requires denied-policy, keyboard,
lifecycle and profile/resolution coverage.

Source, reviewed packaging and installed visibility remain separate gates.
Signed r15 stays frozen; the plugin belongs to a later immutable recipe.

## Evidence

Exact implementation/review receipts will be added before acceptance. Primary
contracts are Codex's [app-server account methods](https://learn.chatgpt.com/docs/app-server)
and Claude's [statusline fields](https://code.claude.com/docs/en/statusline).
