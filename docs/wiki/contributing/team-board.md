# Team board and progress evidence

The QindaQt team board is a live operating view over durable Markdown worker
records, message threads, and the canonical outcome ledger in
`ops/team/features.json`. It deliberately separates delivery evidence from
worker activity.

## Two independent questions

The board answers two questions without mixing them:

1. **Who is working now?** A durable employee record counts only when its owner
   record has parser-valid identity/outcome fields, `- Status: working — ...`,
   and an ISO-dated bullet inside the literal `## Updates` section no more than
   30 minutes old. GPT, Claude, GLM, and all other providers use the same rule.
2. **How much integrated product evidence exists?** Worker count, messages,
   assignments, source-ready branches, compiler activity, review prose, and
   task estimates add zero. Only accepted behavior already integrated at the
   manager boundary can move an outcome step.

Every valid record under `ops/team/workers/` is visible. `ops/team/ROSTER.md`
catalogs stable core personas and staffing intent but is not an allowlist. The
Program Manager enforces the 15-live-process ceiling from direct process,
ownership, and resource evidence; stale roster prose cannot hide a genuine
worker or manufacture liveness.

The three workgroup queues under `ops/team/queues/` show how unfinished Shell,
Platform, and First-party outcomes move from owner to reviewer to integration.
They are coordination evidence only and never affect the product percentage.

## Provider capacity

`ops/team/providers.json` records the five stable provider routes: OpenAI/
Codex, Google Gemini Vertex, Z.AI GLM, Anthropic Claude, and Moonshot Kimi.
Each record declares `available`, `degraded`, or `unavailable`, the last real
probe time, evidence, and an estimated return/retry time when capacity is
impaired. The graphical board renders all five and counts only `available`
routes in its provider-capacity card.

Return times are estimates, not liveness. Reprobe at or before the recorded
time, replace the estimate with observed state, and do not list a token-silent,
quota-rejected, or semantically failed route as available. Provider capacity
never creates worker liveness or product progress; it explains how much of the
staffing system can presently be used.

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
node --test tools/team-board/board.test.mjs tools/team-board/markdown.dom.test.mjs
```

Then start an ephemeral server against an isolated or live team root and check
`/api/board` reconciliation before replacing the long-running local board.

See [Flow team workflow](flow-team-workflow.md) for the delivery/refill loop
that produces the queue and message evidence.
