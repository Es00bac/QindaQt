# QindaQt team roster

This is the current static QindaQt delivery team. The manager is outside the
15-worker ceiling. A worker's name, role, provider, exact model, and reasoning
level are immutable; a different tuple is a different employee. Assignments,
feature crews, supervisors, worktrees, and liveness may change.

Historical employee files remain under `workers/` as preserved delivery
evidence. They are not current roster members unless named below. Never delete
or rewrite their history to make the roster look smaller.

`working` is not a roster property. It is a fresh, self-declared runtime state
in the employee's own Markdown record. Waiting, finished, handoff, paused, and
stale sessions count as zero live workers.

## Current 15 workers

| Employee | Permanent role | Provider/model | Reasoning | Home crew |
| --- | --- | --- | --- | --- |
| Cora Vale | Reusable-controls implementer | OpenAI Codex runtime; exact serving model unexposed | Unverified | Controls |
| Tessa Rowan | Independent QindaQt.Controls S2 exact-candidate reviewer | OpenAI Codex runtime; exact serving model unexposed | Unverified | Controls |
| Dorian Vale | KWin API and nested-session evidence auditor | OpenAI `gpt-5.6-sol` | High | Shell and display |
| Rhea Calder | Display D0 compositor-output lead | OpenAI `gpt-5.6-sol` | High | Display |
| Kellan Ward | Display D1 transaction implementer and lead | OpenAI Codex runtime; exact serving model unexposed | Unverified | Display |
| Mina Shah | Display D1 public-API/docs/acceptance reviewer | Anthropic `claude-sonnet-5` | High | Display |
| Iris Hale | Display D1 adversarial model assistant | GLM `zai-coding-plan/glm-5.3-flash` | High | Display |
| Elara Finch | Display/output architecture analyst and exact reviewer | Anthropic `claude-fable-5` | Maximum | Display |
| Soren Pike | Notification live-session qualification engineer | OpenAI Codex runtime; exact serving model unexposed | Unverified | Notifications |
| Omar Finch | Notification containment QA assistant to Soren Pike | GLM `zai-coding-plan/glm-5.3-flash` | High | Notifications |
| Theo Marsh | Qualification-provenance reviewer to Soren Pike | Anthropic Claude Haiku 4.5 | High | Notifications |
| Linnea Marsh | First-party native Text Editor implementer | OpenAI Codex runtime; exact serving model unexposed | Unverified | First-party apps |
| Rowan Lee | AppShell experience architect | GLM `zai-coding-plan/glm-5.3-flash` | High | First-party apps |
| Juno Park | Native applications design engineer | GLM `zai-coding-plan/glm-5.3-flash` | High | First-party apps |
| Priya Nair | Power and brightness platform architect | GLM `zai-coding-plan/glm-5.3-flash` | High | Platform services |

## Crew relationships

- Controls: Cora owns implementation, candidate commits, and repair decisions;
  Tessa independently reviews the exact immutable candidate. Nia Hart's
  completed same-worktree source-audit engagement remains preserved history.
- Shell: Mira Quill's completed production-surface outcome and employee record
  are preserved as history; Dorian supplies independent exact KWin and nested-
  session review for a future named shell implementer when needed.
- Display: Rhea owns D0. Kellan Ward is the durable identity assigned to the
  previously anonymous `/root/display_d1` implementer/lead and owns D1; Mina,
  Iris, and Elara retain their established review/analysis duties and report
  findings to Kellan. Dorian may audit exact KWin/runtime evidence.
- Notifications: Soren owns the installed live-session outcome; Omar and Theo
  are his direct containment and provenance partners.
- First-party apps: Linnea owns the Text Editor vertical slice. Rowan and Juno
  advise her through the shared app-design thread and may take later AppShell
  or application slices only within their permanent roles.
- Platform services: Priya owns analysis and planning for Power1/Brightness1.
  A future implementation lead must be an existing suitable roster employee
  or a deliberate replacement hire; Priya does not silently become an
  implementer.

## Staffing rules

- The manager assigns complete outcomes and prevents file/resource collisions.
- A direct assistant reads the same feature thread and the supervisor's
  evolving worktree, but edits only paths explicitly delegated by the
  supervisor. Read-only assistants never mutate the product tree.
- Compile-only work may run in multiple isolated worktrees when the manager
  has measured sufficient host headroom; every build remains serial
  `--parallel 1`. Private nested runtime/session work stays single-lane to
  prevent display, socket, bus, input-fixture, and teardown collisions. Other
  workers continue independent source, documentation, architecture, or
  immutable review work.
- Finished workers remain on the team, read the queue and peer threads, and
  take the next suitable outcome or help request. Their status stays truthful
  between invocations.
- The current team never exceeds 15 workers. Replacing a roster member requires
  a dated manager note naming the evidence-based reason and preserves both
  employee records.
