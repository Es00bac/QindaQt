# AI agent usage applet

The built-in **AI agent usage** applet opens beside the existing hardware
status controls. Its popup shows Claude, Codex, Kimi, GLM, DeepSeek and Mistral,
plus providers supplied by valid custom reports. Existing profile geometry and
hardware controls stay as configured; top-bar profiles put usage in their top bar.

Current maturity: implementation candidate; verification evidence is recorded
below when the compiled and policy gates pass. The durable boundary is
[ADR-0351](../adr/0351-collect-agent-usage-through-bounded-metadata-sources.md).

## Facts shown

Each provider retains its own scope, source, observation time and diagnostic.
Token and cost scopes are separately labeled: Claude context tokens must never
be described as session tokens merely because its reported cost covers a session.
Subscription quota windows show **remaining** percent (100 minus the reported
used percent, clamped to zero for an overage) and local reset time.
All report labels render as plain text, including markup-like provider metadata. Missing percentages and reset times say
“not reported.” Total, input and output token counts remain separate. Cost is
explicitly **reported cost in USD**, never inferred from token pricing.

Quota windows lead each row, followed by compact token and cost blocks with
independent scopes. Observation time and provenance remain muted but readable.
Unavailable providers remain visible as concise setup opportunities; rows with
no metrics say “Usage not reported” without repeating every unknown field. Stale reports
remain marked stale with their original observation time; they do not become
current merely because the popup reopened. No provider value is treated as zero
when absent, and there is no cross-provider aggregate that mixes incompatible
session, lifetime or billing-period counts.

## Local report setup

Collection reads bounded normalized reports in
`$XDG_CONFIG_HOME/qindaqt/agent-usage/reports` (normally
`~/.config/qindaqt/agent-usage/reports`). Use the installed
`qindaqt-agent-usage-report` tool to validate/write a provider report, or
normalize the supported Claude statusline payload. The report tool's help
describes its schema and arguments. Configure the provider tool separately;
the shell has no account credential entry or account authorization surface.

Codex collection uses the configured local Codex program's documented stdio
app-server account metadata methods. The backend's primary documentation
specifies which counters are available and their scope. Unsupported subscription limits stay unknown.

## Ownership and lifecycle

- `src/services/agent_usage` owns validated values, collection and local feeds.
- `src/shell/agent_usage_applet` owns pure projection, an injected
  `AgentUsageSource` controller and compiled
  `QindaQt.Shell.AgentUsageApplet 1.0` presentation.
- `AgentUsageAppletComposition` evaluates manifest, audited registry and
  `agent-usage.read` policy before constructing a collector.
- Runtime panel host properties and the built-in dispatcher route the facade
  into every panel instance. The manifest admits horizontal and vertical panels.

Construction does not read reports or invoke providers. Opening the popup or
pressing Refresh explicitly refreshes usage facts. While open, a 60-second
presentation check reprojects the source snapshot's freshness without reading
reports or invoking provider processes; the check stops when closed or denied. Denied policy constructs no
collector, snapshots no source and disables Refresh. Presentation performs no
direct network requests, login, application setting rewrite or service startup;
the Codex CLI retains ownership of its account metadata transport.

The summary button is keyboard focusable and exposes its purpose through an
accessible name and description. Enter/Space or an accessible press opens the
shared [PanelPopup](panel-popup-placement.md); Escape closes it. The popup
scrolls bounded content for compact resolutions and retains text descriptions
for provider state, remaining limits, resets and cost.

## Verification

The focused applet suite covers unknown versus zero, quota projection, reported
cost and scope, controller publication, source lifetime, and no reads under
policy denial. Compiled QML checks cover keyboard and click opening, Escape,
Refresh, accessibility and compact horizontal/vertical panel geometry.
Runtime catalog/resolution checks verify registration and installed profile
placements. Actual provider account limits require provider-supplied reports;
the fixture tests do not claim physical desktop or live account qualification.
