# Agent usage reports

The agent usage applet consumes the
`QindaQt::Services::AgentUsage::AgentUsageSource` boundary. Its owning snapshot
contains optional metrics: missing values are not zero or estimated prices.
The [metadata-source decision](../adr/0351-collect-agent-usage-through-bounded-metadata-sources.md)
defines the read-only process and report boundary.

## Collection and scope

Constructing `AgentUsageCollector(feedDirectory, codexProgram, clock, parent)`
performs no IO. The caller owns the QObject, supplies a nonempty clock function,
and calls snapshot, refresh and destruction on the object's thread. Snapshot
returns an owning copy. Explicit refresh is single flight; another request
during collection is ignored. No daemon or automatic startup collection exists.

Codex uses the injected executable with exactly `app-server --listen stdio://`.
Newline JSON RPC initializes once, sends the initialized notification, then reads
only `account/rateLimits/read` and `account/usage/read`.
It never starts a thread, turn, inference, authentication or credit mutation.
The multi-bucket `rateLimitsByLimitId` view takes precedence over its duplicate
legacy `rateLimits` view. Only percent, reset timestamp and lifetime token count
are projected. Bucket names, account identity, notifications, server requests,
stderr and error text are discarded. Unsupported usage/auth modes leave token
metrics absent while successful quotas survive. Input/output tokens and USD
cost are absent because the documented usage summary does not report them.
See the [official Codex app-server protocol](https://learn.chatgpt.com/docs/app-server).

Claude's optional statusline publisher projects five-hour, seven-day and gateway
spend-limit percentages/reset times, context input/output token counts and
reported session USD cost. Context tokens describe the current context/latest
response; they are not cumulative session totals. No combined total is invented.
Gateway spend percentages can exceed 100; displayed remaining allowance is
clamped at zero. Optional gateway spend dollar estimates are not copied into
session cost or treated as billed amounts.
See the [official Claude statusline contract](https://code.claude.com/docs/en/statusline).
The reported session cost may differ from actual billing.

## Normalized report schema

Reports live in `$XDG_CONFIG_HOME/qindaqt/agent-usage/reports`, with
`~/.config/qindaqt/agent-usage/reports` as fallback. Each filename is
`<providerId>.json`; IDs match `[a-z][a-z0-9-]{0,31}`.
Codex always uses live CLI metadata; `codex.json` never overrides it.
Claude, Kimi, GLM, DeepSeek, Mistral, OpenCode and custom providers use reports;
no undocumented statistics flags or credentials are required.

```json
{
  "schemaVersion": 1,
  "providerId": "kimi",
  "source": "provider-report",
  "observedAt": "2026-10-07T19:00:00Z",
  "scope": "report-period",
  "tokenScope": "session",
  "costScope": "session",
  "totalTokens": 1200,
  "reportedCostUsd": 0.25,
  "quotaWindows": [
    {"label": "daily", "usedPercent": 20, "resetAt": "2026-10-08T00:00:00Z"}
  ]
}
```

This is a schema example, not a provider quota or pricing assertion.
Required identity/source/observation/scope fields must be present.
Sources are `provider-report`, `manual-report` or `claude-statusline`; they
identify the actual producer class, not an independently verified billing API.
Scope values are `lifetime`, `session`, `context`, `billing-period`,
`report-period` or `unreported`. Keep tokenScope and costScope separate.
Optional inputTokens/outputTokens/totalTokens and reportedCostUsd may be absent
or null. Tokens are exact unsigned integers no greater than 2^53-1; USD cost
must be finite, nonnegative and no greater than 10^12.
No token-to-dollar conversion exists.

Quota labels are `five-hour`, `seven-day`, `daily`, `weekly`, `monthly`,
`primary`, `secondary`, `quota` and `spend-limit`. Percent/reset fields are
independently optional. Ordinary percentages are 0–100; spend-limit consumption
has a finite defensive ceiling of 1,000,000 percent. Dates are UTC ISO8601 with
a terminal Z. Observations more than five minutes ahead are rejected.
Reports and successful live observations become Stale after 15 minutes;
missing reports are Unavailable and invalid/read failures are Error.

Unknown keys, duplicate keys, malformed types and nonfinite values are rejected.
Files are capped at 64KiB, quotas at 16 per report, and discovery at 64 inspected directory entries (including unrelated files), at
most 31 custom provider files and at most 40 snapshot rows. Symlinks, multiply linked files, foreign ownership, group/other writable
report directories/files and nonregular files are rejected. An explicit report-feed
Error row reports discovery overflow/omission. Open directory descriptors and
openat avoid pathname races; fstat validates the opened inode before reading.
The collector opens feeds with no-follow/nonblocking semantics and reads at
most 64KiB plus one rejection byte. Subprocess output, including discarded
stderr, is capped at 64KiB and requests expire after five seconds. Destruction
kills the owned process and waits at most one second. During a cold request the Codex detail says Collecting usage metadata. Later
refreshes retain last-known fields/timestamps labelled Refreshing last-known
metadata. A wholly failed refresh publishes those prior fields as Stale with a
fixed last-known failure detail, without advancing observedAt. Partial fresh
responses replace old fields; older totals are never mixed into fresh quotas. Partial
successful quotas survive an unsupported response or process failure with an
explicit partial detail. There is no process
restart loop or retained raw output log.

## Optional publisher setup

After Portage installs the publisher, a provider can pipe its normalized metrics
into:

```sh
qindaqt-agent-usage-report --provider kimi < normalized-metrics.json
```

For Claude, explicitly compose the following into an existing statusline script
that already receives the documented JSON on stdin:

```sh
qindaqt-agent-usage-report --provider claude --claude-statusline
```

The command does not edit Claude settings or install a hook. Preserve existing
statusline behavior by passing the same already-received JSON to both its current
renderer and this publisher; do not read a transcript to supply input.
The publisher projects only allowlisted metrics from statusline JSON. It drops
paths, session identifiers, model metadata and unrelated raw fields. Normalized
input instead rejects unknown fields. It atomically replaces one 0600 report
using a same-directory temporary file, and creates new directories with 0700
permissions. `--directory` is an explicit test/producer override.
The installed publisher is a QtCore executable reusing the service parser;
Python is used only by test fixtures. Stdin is limited to 64KiB and five seconds.
Success is silent; rejection emits only a fixed diagnostic, never raw input.

## Qualification

Focused tests cover malformed/duplicate reports, context/session scopes,
missing metrics, authoritative and legacy Codex buckets, partial unsupported
usage, stale snapshots, single flight, timeout, output cap, missing executable,
bounded destruction, symlink feeds and allowlist-only publisher output.
An explicitly invoked test-only metadata probe prints only state and counts of
available quota/token/cost metrics. It performs no inference and emits no account
or payload contents. These gates do not qualify provider billing accuracy,
credentials, physical desktop interaction or a later package installation.
