# Worker records

Each worker owns only `ops/team/workers/<stable-name>.md`. The record states the
worker's verified provider/model identity, role, reasoning level, status,
outcome, timestamps, branch, and worktree, followed by observed strengths and
dated updates.

`status: working` is valid only while a real process is actively doing that
outcome. Use `reviewing`, `handoff`, `idle`, or `retired` for the corresponding
observed state. Never fabricate provider identity, model selection, liveness,
tests, or completion.
