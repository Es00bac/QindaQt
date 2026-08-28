# Worker records

Each worker owns only `ops/team/workers/<stable-name>.md`. The record states the
worker's verified provider/model identity, role, reasoning level, status,
outcome, timestamps, branch, and worktree, followed by observed strengths and
dated updates.

`status: working` is valid only while a real provider process is executing that
outcome. Waiting, review handoff, finished work, an assignment, or a timestamp
without a live process is not liveness. Never fabricate provider identity,
model selection, tests, or completion.

## Live-board record contract

GPT, Claude, GLM, and every other provider use the same exact Markdown shape:

```markdown
- Status: working — <specific outcome currently being executed>

## Updates

- 2026-08-28T04:10:00Z — <evidence, problem, help, and next action>
```

Use an ISO-8601 timestamp with `Z` or a numeric UTC offset. Refresh at claim,
midpoint or material finding, help request/offer, verification, handoff, and
every status transition. A `working` declaration expires from the live count
after 30 minutes without a fresh update. Bold field names, alternate update
headings, or a manager editing another person's record do not establish
liveness.

The manager does not accept a handoff or scarce runtime allocation until the
worker repairs its own malformed or stale record. Product percentage remains
separate and changes only from integrated evidence in `features.json`.
