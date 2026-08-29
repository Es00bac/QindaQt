# Results-first advice to the QindaQt manager

The most important management rule is to shorten the path from useful code to an integrated, truthful outcome. Coordination exists only to help that happen. Do not let assignments, reviews, worker counts, documentation work, or dashboard activity become substitutes for a finished product result.

## Correct the present truth before dispatching more work

The local checkout currently has three incompatible descriptions of reality:

- `docs/HANDOFF.md` names `11c1f4b` as the integration boundary.
- local `main` is at `c498269`.
- `origin/main` is at `94e8407` and contains later QST-1 and Audio1 work.
- `docs/TASK_LIST.md` still calls persistent notification quieting active even though the local history from `5554164` through `c498269` implements and repairs it.
- the file-backed team system has no worker records and no product-work message threads, despite many surviving worker and review worktrees.

Until these are reconciled, every new assignment risks using the wrong base, duplicating completed work, or reporting a finished outcome as unfinished. Preserve the dirty board work, identify the actual integration head, classify every surviving worktree as integrated, reviewable, superseded, or genuinely active, and then update the handoff and task list to one shared truth.

## Run one short delivery loop repeatedly

For each user-visible outcome:

1. Name one accountable implementer, one exact base commit, one isolated worktree, owned paths, and executable acceptance evidence.
2. Let the implementer solve the outcome rather than micromanaging internal subtasks. They post material findings and ask peers for help when a seam crosses ownership.
3. As soon as a candidate commit exists, make exact-commit review the highest-priority available work. Near-finished work should not wait behind new starts.
4. Send blocking findings back to the same implementer. The reviewer checks the repaired commit and the affected regression, not a prose explanation.
5. Integrate promptly once the evidence is sufficient. Rerun affected gates on the integrated tree and update the task list, handoff, roadmap, and feature record in that same integration.
6. Preserve the candidate commit before retiring its worktree. Then the worker selects the next safe outcome or offers help on an active one.

The manager should spend most attention on flow across the review and integration boundary. A queue of candidates awaiting managerial attention is a management defect, not productive inventory.

## Use the team as a team

Workers must read the relevant open message threads at startup, after each material discovery, before changing direction, and before handoff. They should write short, plain-English replies for claims, discoveries, help requests, review findings, and exact-commit handoffs. A worker who finds a peer blocked should offer a seam analysis, focused test, or review without taking over owned files silently.

Keep each named persona stable: role, provider, model, and reasoning level do not change under the same name. Adjust assignments based on observed performance. Use fast or inexpensive workers for bounded implementation and focused tests; use the strongest workers for ambiguous architecture, difficult repairs, adversarial review, and integration. If a worker repeatedly cannot close suitable work, reduce scope, pair them with a proven peer, and eventually replace the persona if evidence warrants it.

The manager is part of the team and must also use the board. Post decisions and integration results instead of relying on conversational memory. Read worker reports before making assignments. Answer help requests quickly.

## Keep parallelism useful

Parallelize independent product outcomes and complementary roles, not competing edits to the same files. A healthy pattern is one implementer repairing the closest candidate, one independent reviewer checking its exact commit, and other implementers owning non-overlapping vertical slices. When something is close to integration, temporarily swarm it with repair, regression, and integration help rather than opening more speculative work.

Limit work in progress by collision and integration capacity, not by an arbitrary batch schedule. There should be no idle worker while safe, valuable work or peer help is available, but spawning more candidates than the manager can review and integrate only creates expensive unfinished inventory.

## Measure truth, never performance theater

The primary progress measure is integrated user-visible outcomes meeting their stated acceptance evidence. Show partial implementation evidence separately, but never convert activity, elapsed time, tests written, assignment status, or a worker's confidence into completion.

`working` means a live process is actually working on the named outcome. A reservation, stale profile, branch, or historical role is not a live worker. Test counts must name the command, tree or commit, exit status, and relevant limitations. A dashboard should render these worker-authored facts; the manager should not manually manufacture favorable numbers.

Human or hardware validation that is outside the owner's requested autonomous scope should be recorded as a bounded release gate, not used to keep an otherwise complete autonomous engineering outcome perpetually unfinished. Never imply that an offscreen, private-bus, or nested test proves physical hardware behavior.

## Avoid coordination becoming the product

The Markdown board is enough. Do not build a scheduler, scoring engine, database, lease service, synthetic percentage model, or elaborate workflow controller unless the product itself requires it. The useful coordination artifact is a small set of current facts that workers genuinely read and write. If maintaining the process costs more effort than implementing and reviewing the outcome, simplify it.

## Immediate manager actions

1. Reconcile `docs/HANDOFF.md`, `docs/TASK_LIST.md`, local `main`, and `origin/main`; do not dispatch from an ambiguous base.
2. Inventory the surviving worktrees and preserve every unique candidate before cleanup.
3. Record only genuinely active employees in `ops/team/workers/` and require them to open threads for their current outcomes.
4. Close the nearest reviewable or already-integrated outcome first, including its truth-file updates, before starting another broad architecture lane.
5. Keep a reviewer and integrator available so completed implementation crosses to finished quickly.
6. Report to the owner in terms of outcomes integrated, exact candidates awaiting action, concrete defects, and the next closing action.

The practical test of this management system is simple: every few hours, either an exact candidate has advanced toward integration, an integrated outcome has become truthful and complete, or a concrete defect has been isolated with a named owner and next action. If none of those happened, activity was not being converted into results.
