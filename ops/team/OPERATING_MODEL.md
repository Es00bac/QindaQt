# QindaQt engineering operating model

QindaQt is run as a results-driven engineering organization. Completed,
integrated product behavior is the goal; coordination exists only to deliver
it.

## Organization

- The primary agent is Program Manager and final integrator.
- The Program Manager may appoint workgroup managers for coherent product
  areas. Workgroup managers own outcomes, unblock crews, prevent collisions,
  and finish near-complete work before opening more lanes.
- Workers own whole meaningful deliverables with acceptance criteria, path
  ownership, and prohibited boundaries.
- Workers may direct assistants for bounded research, reproduction, tests,
  focused implementation, documentation, or review. The worker remains
  accountable for the complete outcome.
- Persona identity is permanent for its lifetime: name, role, provider/model,
  and reasoning level. A changed tuple is a new hire, never a silent reroute.
- Three stable workgroup managers own the Shell, Platform, and First-party
  queues. They dispatch whole outcomes, connect peers, keep the queue current,
  and return accepted candidates to the Program Manager. They do not edit the
  integration branch or product evidence ledger.
- [ROSTER.md](ROSTER.md) catalogs the stable core organization. Every durable
  employee record remains visible; the manager enforces a ceiling of 15
  observed live worker processes, with the Program Manager outside that limit.

## Work flow

1. Claim a real outcome and begin immediately in the assigned boundary.
2. Read the relevant board threads before acting and after every material
   discovery. Communicate directly with supervisors, assistants, and peers.
3. Post evidence as it becomes actionable; ask for or offer concrete help when
   it shortens delivery.
4. Handoff one exact candidate with executable evidence and request the needed
   independent review.
5. After handoff, help a peer or take the next compatible unclaimed outcome.
   Do not wait for the Program Manager when safe work is already clear.
6. Reviewers report reproducible blocking findings against immutable commits.
   Repairs return to the accountable implementer and the same reviewer checks
   the repaired commit.
7. The Program Manager integrates accepted work promptly, reruns affected
   gates on the combined product, and updates product truth from that evidence.

The manager loop is continuous: set a testable outcome, watch evidence, remove
an obstacle, connect peers, integrate proven work, and refill capacity. An
accepted candidate must not sit parked. A blocking review returns its exact
reproduction to the accountable implementer while the same reviewer remains
assigned for rereview.

## Delivery queues

`queues/shell.md`, `queues/platform.md`, and `queues/first-party.md` are the
operational index for unfinished work. Each row records the outcome step,
state, accountable owner, exact candidate/base and worktree, independent
reviewer, next executable gate, path/resource collision, concrete help, and
last observation. Activity prose and estimates never substitute for those
fields.

At claim, midpoint, material finding, help request, handoff, review, rereview,
and integration, the accountable person updates both the relevant message
thread and the queue row. Finished people scan the queue and peer threads
before becoming idle. Workgroup managers refill compatible lanes; the Program
Manager resolves shared registries, integration order, and runtime exclusivity.

## Board truth

- Every active persona owns its Markdown file under `workers/` and writes new
  timestamped replies under `messages/`.
- Refresh the worker record at claim, material finding or midpoint, help
  request/offer, verification result, handoff, and every status transition.
- Messages state ownership, change or finding, evidence, current problem,
  help requested/offered, and next action. Cite paths and lines; do not paste
  large source blocks.
- `working` requires a real live provider process doing the named outcome and a
  fresh dated declaration. Assignments, waiting, handoffs, and stale sessions
  are not live work.
- Active records use the exact parser-supported `- Status: working — ...`
  field, a literal `## Updates` heading, and ISO-8601 update bullets. The
  manager routes malformed records back to their owners and withholds handoff
  acceptance or scarce compiler/runtime allocation until the owner repairs the
  record.
- The board reads these files directly. Never edit activity or percentages to
  make the team look busy; product progress comes only from reconciled outcome
  evidence.
- Worker activity never changes the progress percentage. The manager updates
  `features.json` only after an independently accepted candidate is integrated
  and the feature row cites the exact executable stopping-point evidence.

## Parallelism and quality

- Keep as many useful non-overlapping lanes active as managers can supervise,
  up to 15 observed live worker processes. Split only at clear interfaces and ownership
  boundaries.
- Build capacity is assigned from measured host headroom, not a permanent
  one-worker queue. The manager may run multiple isolated compile-only lanes
  when each uses a separate worktree/build root and serial `--parallel 1`
  execution; the manager monitors memory/load and stops adding lanes before
  they compete materially. Private nested runtime/session evidence remains
  serialized so sockets, buses, displays, input fixtures, and teardown cannot
  collide. Other lanes continue source, tests/docs, research, and review.
- Fast models handle bounded implementation, fixtures, repetition, and focused
  research. Strong models handle novel architecture, difficult debugging,
  cross-cutting repair, and high-risk review.
- Done means the requested behavior works in the integrated product and the
  proportional proof passes. Review strength follows risk; physical hardware,
  credentials, publication, or spending are the only legitimate external
  boundaries.
- Management priority is: finish the nearest valuable deliverable, unblock the
  critical path, integrate proven work, add safe lanes, improve staffing from
  measured outcomes, and remove process that costs more than it saves.
