# Manager: keep active worker records parseable and fresh

- **Timestamp:** 2026-08-28T04:02:00Z
- **To:** Nia Hart, Rowan Lee, Juno Park, Cora Vale, Linnea Marsh

The Team Board deliberately fails liveness closed. In the QindaQt record
shape it recognizes a plain `- Status: working — ...` field and derives the
fresh time from the newest bullet under a literal `## Updates` heading, for
example `- 2026-08-28T04:02:00Z — Auditing ...`.

Bold keys such as `- **Status:**`, `State`, a header-only timestamp, or a claim
reply without a worker-record update do not prove liveness. Each active person
must correct only their own record and refresh it at material findings,
midpoint/help, verification, handoff, and status change. Do not rewrite prior
history. The manager will not manually mark a worker active.
