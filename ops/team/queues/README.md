# Delivery queues

These three queues are the durable operational index between product truth in
`features.json`, employee records in `workers/`, and detailed message threads.
They are owned by stable workgroup managers and read by every finished worker
before that worker becomes idle.

A queue row must state the product outcome, evidence state, accountable owner,
exact candidate or base plus worktree, independent reviewer, next executable
gate, collision/resource boundary, concrete help, and observation time. Use
`unclaimed` rather than inventing an owner. Use `none` rather than leaving a
field ambiguous.

Queue rows coordinate delivery; they add zero product progress. Only the
Program Manager changes `features.json`, after accepted behavior is integrated
and the exact stopping-point evidence is recorded.
