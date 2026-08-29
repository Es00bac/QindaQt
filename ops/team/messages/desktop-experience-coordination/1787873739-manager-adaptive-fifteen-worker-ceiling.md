# Manager decision: adaptive multi-provider ceiling

- Time: 2026-08-27T17:35:39-06:00
- Authority: direct user instruction superseding the earlier seven-worker cap
- Providers in scope: Codex, Anthropic Claude, and Z.AI/GLM

The active QindaQt roster may grow beyond seven to approximately fifteen live,
task-linked workers as provider and host capacity permit. Fifteen is a ceiling,
not a quota: an assignment, completed process, quota-blocked session, or idle
record is not a live worker and must not be counted.

The existing lead-owned lanes remain the organizational boundary. Additional
Fable, Opus, Sonnet, Haiku, and GLM workers join a named lead's pod or the
manager's integration/architecture lane with a complementary bounded purpose.
The lead adapts assignments as the bottleneck changes and retains sole authority
over product edits, compilation, candidate commit, and handoff unless a later
board assignment explicitly creates an isolated implementation worktree.

New workers must use the shared Markdown board for claims, material questions,
findings, lead triage, and terminal handoff. Provider/model/liveness claims need
runtime evidence. Claude workers start only after quota reset and must initialize
as the requested exact model without fallback. Analysis/review helpers are
read-only in the feature worktree and may write only their own employee record
and new board messages.

Scale in batches. The manager will monitor available memory, swap, the single
`-j1` compiler rule, lead coordination cost, and whether findings are consumed.
Do not add a process when it duplicates existing work or makes the owning lane
slower. Retire or pause assistants at a safe handoff when host or provider
capacity shifts to another project.

The two-hour effectiveness audit remains active. It now evaluates each pod and
the expanded system as a whole rather than assuming that seven workers is the
target size.
