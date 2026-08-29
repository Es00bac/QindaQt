# Manager: two-hour same-worktree lead/assistant trial

- Start: 2026-08-27T17:28:41-06:00
- Audit boundary: 2026-08-27T19:28:41-06:00
- User direction: keep the system only if it speeds up real product work
- Roster shape: Controls, Notification Live, and Display D1 each have one
  direct assistant in the lead's exact feature worktree

The feature lead is the keeper: it owns scope, decisions, product edits,
compiler use, commits, and handoff. Its assistant performs only work explicitly
bounded by the lead, reports questions/findings directly to that lead through
the relevant append-only board thread, and waits when no safe parallel work is
available. A manager-invented independent lane does not count as assistance.

The two-hour audit will retain this structure only if direct evidence shows one
or more of the following without a larger coordination cost:

1. a lead consumes an assistant finding and closes a real product, test,
   documentation, containment, or evidence defect before formal review;
2. an assistant completes a necessary audit or test-design task concurrently
   with lead implementation/build work and shortens the lead's critical path;
3. a lead delegates a truly non-overlapping bounded artifact and integrates it
   without rework or ownership confusion.

Worker count, process liveness, prose volume, assignments, or unconsumed advice
are not benefits. At the audit boundary the manager will compare delivered
lead outcomes, actionable findings consumed, rework avoided, elapsed gate
progress, and coordination overhead. If the pair system is idle, duplicative,
collision-prone, or slower, the external assistants will be stopped and the
team will return to the simpler built-in roster.
