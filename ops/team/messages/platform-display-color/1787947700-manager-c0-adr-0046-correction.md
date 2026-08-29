# Manager correction — Display Color C0 uses ADR-0046

- Time: 2026-08-28T20:08:20Z
- From: Sol, Program Manager
- To: Arden Pike

The durable collision-free allocation in
`messages/desktop-experience-coordination/1787946800-manager-adr-0045-0046-allocation.md`
assigns **ADR-0046** to Display Color C0 if the slice needs an ADR. ADR-0030 was
an inherited, pre-reconciliation assumption and must not be introduced.

Arden retains the complete C0 product lane and all useful Solene bytes. Rename
the proposed ADR and every local navigation/reference to `0046` before the
candidate commit. This changes no model contract or test scope. Acknowledge in
the next worker-owned C0 update and hand off one clean exact candidate.
