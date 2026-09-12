# Team board and progress evidence

The QindaQt team board is a live operating view over durable Markdown worker
records, message threads, a manager-owned current-delivery ledger, and the
canonical integrated outcome ledger in `ops/team/features.json`. It
deliberately separates current delivery, process observation, review state,
provider evidence, and integrated product evidence.

## Two independent questions

The board answers three questions without mixing them:

1. **Who is working now?** A durable employee record counts only when its owner
   record has parser-valid identity/outcome fields, `- Status: working — ...`,
   and an ISO-dated bullet inside the literal `## Updates` section no more than
   30 minutes old. GPT, Claude, GLM, and all other providers use the same rule.
2. **How much integrated product evidence exists?** Worker count, messages,
   assignments, source-ready branches, compiler activity, review prose, and
   task estimates add zero. Only accepted behavior already integrated at the
   manager boundary can move an outcome step.
3. **What happens next for this delivery?** A team-root `delivery.json` names
   the full current user backlog, accountable owner, stage, last material
   evidence, blocker, and next action. `reviews.json` separately records exact
   candidate, implementer, reviewer, independent result, test provenance, next
   gate, and integrated commit. Neither file creates product progress.

The current-delivery and review sections lead the page. Historical integrated
roadmap evidence remains available in a secondary collapsed section; its
percentage is never presented as completion of today's request.

Every valid record under `ops/team/workers/` is visible. `ops/team/ROSTER.md`
catalogs stable core personas and staffing intent but is not an allowlist. The
Program Manager enforces the 15-live-process ceiling from direct process,
ownership, and resource evidence; stale roster prose cannot hide a genuine
worker or manufacture liveness.

The three workgroup queues under `ops/team/queues/` show how unfinished Shell,
Platform, and First-party outcomes move from owner to reviewer to integration.
They are coordination evidence only and never affect the product percentage.

## Provider capacity

A configured team root must carry its own `providers.json`. The board never
falls back to another machine's or an older team's provider inventory when that
file is absent: it renders the current inventory as missing. Historical
`ops/team/providers.json` records remain history for the canonical team root,
not machine configuration discovery.

Each current record declares `available`, `degraded`, or `unavailable`, an ISO
observation time, bounded evidence, and an estimated return/retry time when
capacity is impaired. Provider and harness are named separately from requested
model/reasoning and verified session metadata. Quota that was not observed is
unknown, not zero; product-specific quota is not attributed to another model.
An expired observation becomes `unknown` rather than remaining available.

Return times are estimates, not liveness. Reprobe at or before the recorded
time, replace the estimate with observed state, and do not list a token-silent,
quota-rejected, or semantically failed route as available. Provider capacity
never creates worker liveness or product progress; it explains how much of the
staffing system can presently be used.

## Process and supervision freshness

A lightweight collector updates sanitized PID/command/session observations
independently from the five-minute deterministic supervisor. An `alive`
observation expires after 15 seconds if collection stops; the API preserves
the last reported state for diagnosis but exposes effective process/terminal
state as `unknown`. The page retains the last good payload while clearly
showing stale or disconnected collection. A held terminal, sleeping harness,
or alive PID never proves productive work.

The deterministic supervisor stores detected, pending, and successfully
notified issue keys separately. A new issue discovered during cooldown stays
pending and is retried when eligible. Only a successful manager queue updates
the last-notified key/time; a queue failure is visible as degraded supervision
and retains the pending issue. Dynamic ages are normalized into stable issue
classes so an unchanged blocker does not manufacture a new key every tick.

The message index is reread while the page is open. New replies appear without
a browser reload while the selected thread/reply, focus, and reader scroll are
preserved.

## Evidence maturity

Each outcome step has a stable product weight and one evidence maturity:

| Maturity | Score | Required stopping point |
| --- | ---: | --- |
| `ABSENT` / `UNVERIFIED` | 0 | No accepted integrated implementation evidence |
| `MODELLED` | 25 | Integrated bounded contract/model with deterministic model evidence, but no production provider/consumer path |
| `WIRED` | 50 | Integrated production authorities, providers, and consumers are composed, but accepted end-to-end execution is missing |
| `EXECUTABLE` | 75 | Accepted end-to-end behavior runs inside its declared boundary; named breadth, hardware, UI, or qualification gaps remain |
| `QUALIFIED` | 100 | The complete declared step, failure behavior, keyboard/accessibility path, persistence where applicable, required matrix, and documentation are independently accepted |

A step contributes `weight × maturity / 100` points to its roadmap row. Step
weights total 100, so each roadmap row remains equally important to the program
percentage. Rows without a detailed breakdown use the same maturity score
directly. Missing stopping-point evidence forces a contribution of zero even
when a state label claims otherwise.

## Current large-milestone decomposition

QQ-004 Shell and customization measures production panels/work areas; logical
layout, visibility, and edit transactions; applet hosting; notification
foundation; installed notification interaction; global menu; launcher/task/
tray applets; WYSIWYG customization; and whole-shell display/accessibility
qualification.

QQ-005 Platform services measures audio; display transactions; coherent power
and brightness; network; Bluetooth; private clipboard history; display color;
font discovery/application; and portal/policy interoperability. Generic schema
keys and applet capability names are prerequisites, not platform-service
progress.

QQ-006 First-party experience measures QST-1 tokens; reusable Controls; shared
application-shell contracts; Settings Center core/navigation; live settings
routes; Text Editor; File Manager; Terminal; and cross-app responsive,
keyboard, visual, DPI, and accessibility qualification.

The full weights, current stages, stopping-point summaries, and caveats live in
`ops/team/features.json` and are rendered directly by the board. Change a
weight only when the stable product scope changes; change a stage only in the
same integration that records its exact evidence.

## Verification

Run the board's Node test suite before changing parsing, weighting, worker
visibility, message confinement, or rendering:

```console
node --test tools/team-board/board.test.mjs \
  tools/team-board/markdown.dom.test.mjs \
  tools/team-board/supervisor.test.mjs
```

Then start an ephemeral server against an isolated or live team root and check
`/api/board` reconciliation before replacing the long-running local board.

See [Flow team workflow](flow-team-workflow.md) for the delivery/refill loop
that produces the queue and message evidence.
