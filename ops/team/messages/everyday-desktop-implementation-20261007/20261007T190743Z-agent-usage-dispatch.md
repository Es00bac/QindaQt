# AI agent usage applet implementation packet

- Owner outcome: clickable panel icon beside hardware status controls showing coding-agent remaining limits/reset times AND token/reported-cost totals.
- Verified exact base: ccc99fb3f67c2bdc4f7156af7c0b0f1e05478057. Earlier supplied full hash was transcription error; manager/UI HEAD verify this object; no source reset.
- UI: Everyday Agent Usage Sol; worker/everyday-agent-usage-20261007; isolated qinda container-wm.worktrees/everyday-agent-usage-20261007.
- Backend: Everyday Platform Sol; worker/everyday-agent-usage-backend-20261007; isolated qinda container-wm.worktrees/everyday-agent-usage-backend-20261007.
- Reviewer: Everyday Review Sol, exact candidates and combined consumer.
- Manager: Everyday Orchestrator Codex; integration branch only.
- Proposed ADR0351 defines the source/report boundary. r15/bef80e861 freeze stays immutable.

## Ownership

Backend owns src/services/agent_usage/, tests/services/agent_usage/, installed report publisher and docs/wiki/reference/agent-usage-reports.md. Public QindaQt::AgentUsage target, QindaQt::Services::AgentUsage namespace, agent_usage_types.h/source.h/collector.h headers. Same-thread QObject AgentUsageSource provides owning snapshot(), explicit refresh() and snapshotChanged(); constructor performs no I/O. Collector takes feed directory, Codex program and clock explicitly. Nullable metrics, tokenScope/costScope, provenance/timestamp and Unavailable/Ready/Stale/Error remain typed.

UI owns src/shell/agent_usage_applet/, new runtime/agentusageappletcomposition.h/.cpp, tests/shell/agent_usage_applet/, data/applets/agent-usage.json and docs/wiki/shell/agent-usage-applet.md. Approved minimal additions: shell build, runtime application/panel window factory context, compiled BuiltinAppletContent/AppletChip/PanelContent/PanelAppletRow/RuntimePanel host bindings, applets capability type/name, builtin registry, installed profiles next to existing hardware/status group. Preserve each profile design. Root owns ADR/nav/module-boundary docs and integration. Backend/UI coordinate additive src/CMakeLists.txt and tests/CMakeLists.txt registrations.

## Executable acceptance

- Real Codex stdio account metadata RPCs: initialize/initialized, account/rateLimits/read and account/usage/read only. No thread/turn/inference, auth mutation, credit resets, credential copies or transcript reads.
- Claude documented statusline fields normalize through optional installed publisher; never replace existing hooks/settings. Strict normalized report feeds support Kimi/GLM/DeepSeek/Mistral/custom providers with provenance and actual metrics.
- Private XDG_CONFIG_HOME/qindaqt/agent-usage/reports persistence: bounded allowlist, atomic publication, explicit observation/expiry, hostile-file/parser/resource tests, no raw input retained.
- Separate token/cost scopes: Claude context token counts differ from session cost; Codex lifetime tokens have no inferred cost. Unknown is absent, remaining quota only reported, no pricing estimates.
- Deliberate popup open/Refresh triggers collection; denied read capability performs zero file/process collection. Construction/startup performs no I/O.
- Focused backend lifecycle/parser/expiry tests, injected projection/policy tests, compiled QML keyboard/popup lifecycle and profile/resolution coverage; exact independent review then manager integrated gates.
- Compiler remains -j24 -l24, one owner. Root protocol3/3 passed and released lease to Backend; UI coordinates next.
- No software installation, active desktop restart, physical lock/suspend, binhost publication or Origin edits. Portage owns later installation.
- Own live-board records/messages at claim/material/verification/handoff, exact candidate+paths+exit/count evidence, push explicitly to qinda local hub only.

Dispatched 2026-10-07T19:07:43+00:00.
