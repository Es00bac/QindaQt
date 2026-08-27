# QindaQt Agent Instructions

## Scope and source of truth

These instructions apply to the entire repository unless a deeper `AGENTS.md`
narrows them. Start with the [project wiki](docs/wiki/index.md), then read the
pages for the module and behavior being changed. Architecture in the wiki is
normative; an implementation that disagrees with it must update the
documentation or add an ADR in the same change.

## Modular implementation

- Give every module one cohesive responsibility and an explicit public
  boundary. Keep policy, persistence, platform integration, and presentation in
  separate components.
- Do not create god objects or central files that accumulate unrelated
  behavior. Prefer small collaborators with constructor-visible dependencies
  over global state, service lookup, or cross-module friendship.
- Keep implementation details private to their owning module. Cross boundaries
  through the interfaces described in
  [Module boundaries](docs/wiki/architecture/module-boundaries.md); do not reach
  into another module's private headers, QML internals, or storage.
- A hand-written production source file reaching 500 non-blank lines requires a
  decomposition review. Do not introduce one over 600 lines without a prior ADR
  explaining why splitting it would make cohesion worse. Generated and vendored
  files are exempt and must be clearly marked.
- Split tests by behavior and failure mode. A large test file is not a reason to
  combine unrelated fixtures or duplicate setup.
- Public interfaces must state ownership, lifetime, threading, error behavior,
  and compatibility expectations when those are not obvious from the type
  system.

See [Coding practices](docs/wiki/development/coding-practices.md) for the working
rules and [Window containers](docs/wiki/architecture/window-containers.md) for
the core model invariants.

## Comments for future agents

Write source comments for an AI agent that must safely change the code after
losing the original task context. Do not narrate syntax or restate a function
name. Capture intent, non-local constraints, invariants, ownership, and traps.

Use these searchable markers with the native comment syntax of the language:

- `AGENT-NOTE:` explains why a surprising design exists and links to its source
  of truth.
- `AGENT-GUARD:` states an invariant that a nearby edit must preserve and the
  failure caused by violating it.
- `AGENT-CONTRACT:` records a requirement shared across module or process
  boundaries, naming both sides where possible.

Keep marked comments concise and current. A stale marker is a defect; update or
remove it when the associated constraint changes. Use an ADR or wiki page,
rather than a long comment, for durable architectural explanation.

## Documentation is part of the change

- Update every affected wiki page in the same change as behavior, architecture,
  schema, interface, workflow, test matrix, or operational changes.
- Add an ADR for a durable cross-cutting choice, new dependency, changed process
  boundary, persistence contract, or reversal of an accepted decision. Do not
  rewrite history; supersede the prior ADR.
- Add new pages to `mkdocs.yml`, maintain reciprocal links where useful, and run
  `mkdocs build --strict` plus the repository link checker before handoff.
- Code changes that leave relevant documentation or accepted ADR consequences
  inaccurate are incomplete.

The complete policy and ADR workflow are in
[Documentation maintenance](docs/wiki/contributing/documentation-policy.md).

## Ownership and parallel work

- Treat the paths assigned to another active agent as owned by that agent. Do
  not edit them without direct coordination.
- Before editing, inspect the current tree and preserve unrelated work. Never
  replace, revert, or reformat another contributor's changes as cleanup for your
  task.
- A module owns its implementation, public API, focused tests, and primary wiki
  page. Consumers request changes through that public boundary rather than
  patching around it.
- Shared registries and build files are coordination points. Make the smallest
  additive edit needed, and notify affected owners when changing dependency
  direction, public interfaces, schemas, or test harness conventions.
- Keep temporary output in ignored build/cache directories. Never commit local
  session state, generated screenshots outside approved baselines, credentials,
  or machine-specific paths.

## Team workflow

The manager/worker workflow is deliberately file-based. The product source of
truth is [Task list](docs/TASK_LIST.md), the current integration boundary is in
[Handoff](docs/HANDOFF.md), and worker communication lives under
`ops/team/messages/`.

- The manager owns priorities, collision avoidance, candidate integration, and
  truthful outcome reporting. Only the manager edits the integration branch.
- Every implementer receives one user-visible outcome, an exact base commit,
  an isolated Git worktree and branch, explicit path ownership, and executable
  acceptance evidence. Workers never implement in the shared checkout.
- A worker posts a new timestamped Markdown reply when claiming work, finding a
  material fact, requesting or offering help, changing direction, verifying a
  candidate, or handing it off. Never rewrite another worker's reply.
- `ops/team/workers/<name>.md` describes one stable employee identity and its
  genuine current state. `working` requires a live process doing the named
  outcome; an assignment, timestamp, or completed handoff is not liveness.
- Candidate handoffs name one exact commit, changed paths, tests with exit
  status/counts, remaining bounded caveats, and the requested next action.
- A different worker reviews the exact candidate commit before high-confidence
  integration. The implementer repairs blocking findings in the same worktree;
  the reviewer rechecks the repaired commit rather than approving prose.
- The manager integrates an accepted commit, reruns the affected gates on the
  integrated tree, updates the wiki/task state in the same integration, and
  retires a worktree only after its commit is preserved.
- Provider, model, test, process, and completion claims require direct evidence.
  Never infer them from a command line, assignment, exit code, or stale record.

Workers may use the collaboration channel for prompt delivery, but durable
claims and handoffs still belong on the message board. Coordination must remain
smaller than the product work it enables.

## Verification

Build and run focused tests for every changed module, then run the broadest
available suite appropriate to the change. Model mutations require invariant and
round-trip tests; shell changes require profile/resolution coverage; compositor
and display changes require the nested scenarios described in the
[testing harness](docs/wiki/development/testing-harness.md). Report commands run
and any unavailable coverage at handoff.

## Milestone commits

- Create a Git commit when a roadmap milestone is complete and its acceptance
  evidence passes. Do not mix unfinished work for the next milestone into that
  boundary commit.
- Use an imperative subject naming the outcome. The commit body must tell a
  future agent what contracts landed, which verification gates passed, and
  what architectural boundary remains next.
- Intermediate commits are appropriate for independently reviewable vertical
  slices, but never label a milestone complete before the wiki roadmap and
  requirement audit agree with the implementation.
