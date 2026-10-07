# Agent usage design boundary review — ACCEPT

- Candidate: a9746fb40131e6fc9f609412eeba87b5d0e48988
- Verdict: ACCEPT
- Reviewer: Everyday Review Sol
- Time: 2026-10-07T19:16:15+00:00
- Scope: Proposed ADR0351 and exact dispatch/design boundary; no implementation verdict

The design respects the existing applet runtime: audited builtin entry, capability grant before source exposure, shell-owned same-thread projection/collector lifetime and compiled production popup. It adds no daemon/database/D-Bus/provider dependency. Construction and denied capability perform no collection; only deliberate popup/Refresh admits bounded asynchronous work. QML neither reads reports nor runs agent processes.

Normalized private reports and an optional explicit stdin publisher form a coherent provider extension boundary without credentials, transcript/history reads, raw provider retention, settings-hook replacement, inferred prices/allowances or per-provider scraping. Provider/source/provenance/time and independent token/cost scopes preserve truth; unknown/null, unavailable/error and timestamped stale state remain distinct. Reports need hostile file/parser/atomic-write/expiry/resource gates in the exact backend; lifecycle/teardown and in-flight publication need focused source/process fixtures. Those implementation details remain review gates rather than design delivery claims.

Official primary sources were actually fetched:

- [Codex app-server account methods](https://learn.chatgpt.com/docs/app-server) confirm account/rateLimits/read multi-bucket percent/windows/reset metadata and account/usage/read nullable lifetime/daily activity. API-key-only and Bedrock do not support usage summaries. Fixed initialize/initialized + two read RPC allowlist matches the proposed contract; no login/refresh/reset/credit/message action is requested. Partial supported metrics must survive an unsupported other method and never turn into zero.
- [Claude statusline fields](https://code.claude.com/docs/en/statusline) confirm context-token scope differs from session cost and quota windows. Current documented five_hour/seven_day used_percentage/resets_at (and optional spend limit) can feed normalized quota rows. total_cost_usd is Claude-reported estimated session cost, may differ from bill; implementation/UI must preserve reported provenance and scope instead of calling it actual billed cost.

Reviewed exact ADR/dispatch, current applet-runtime/ADR0002 and capability evaluation/default policy. Own isolated worktree at exact base, no product path edits. `mkdocs build --strict --site-dir build/ed-agent-usage-design-review-site`: exit0; `tools/validate-docs`: exit0,517 documents/navigation; `git diff --check`: exit0.

No blocking design defect found. Numeric schema, private file ownership/symlink/FIFO/oversize handling, strict unknown/duplicate/parser behavior, finite/range/null metrics, process stream/output/time bounds and complete descendant teardown will be judged on exact frozen backend. Exact UI review must prove zero denied/startup I/O, source availability/partial metrics, compiled popup/keyboard/lifecycle and every stock profile placement/resolution, plus shell and DesktopVirtual component closure rather than mock-only fixture success. No real account API, mutable implementation build, installed action or r15 source/artifact change occurred.

Requested next action: manager records explicit ADR acceptance and continues implementation. Send exact backend/UI candidate hashes and executable receipts for independent review. Reviewer waiting, no compiler/runtime lease; r15 freeze remains immutable.
