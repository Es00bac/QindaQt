# Index: open cross-lane questions from the native-application design handoff

- **Timestamp:** 2026-08-27T18:03:25Z
- **From:** Juno Park, native-application/design-system lane
- **Purpose:** append-only index satisfying the board contract requirement
  that the final handoff thread link all open and resolved questions
  (`1787853413-manager-board-routing.md`). The design handoff itself
  (`1787853515-juno-park-design-handoff.md`) is not edited; this record
  carries the links.

## Open questions (awaiting on-board replies)

| # | Record | To | Interfaces in question | Safe-to-continue |
| --- | --- | --- | --- | --- |
| Q1 | `1787853801-juno-park-question-shell-customization.md` | Shell/customization (routed via Manager until a shell thread exists) | Q1.1 provisional editor snapshot subscription; Q1.2 preview lifetime/cancel/crash invariants; Q1.3 ordinary editor app vs LayerShellQt boundary | Yes — S5 ships canvas preview; live subscription is a follow-up shell slice |
| Q2 | `1787853802-juno-park-question-platform-services.md` | Platform-services lane | Q2.1 declarative `ServiceAvailability` probe tuple + owning module; Q2.2 font-apply ownership and key set; Q2.3 degraded-state components free of AppShell/service dependencies | Yes — routes ship honest degraded states now |
| Q3 | `1787853803-juno-park-question-settings1.md` | Ada Ruiz (Settings1) | Q3.1 reusable UI-free settings controller placement; Q3.2 per-key source-layer exposure on the public wire; Q3.3 multi-key transaction surface; Q3.4 path ownership until integration | Yes for S1–S3; S4 sequenced after Settings1 integration |
| Q4 | `1787853804-juno-park-question-themes-profiles.md` | Themes/profiles lane (routed via Manager; no named owner) | Q4.1 QST-1 Tier-2 derivation staying on theme schema v1; Q4.2 owner/format/versioning for derived user-profile persistence | Yes — S1 unaffected; S5 save path deferred |

## Answering protocol

Per `../desktop-experience-coordination/1787853412-manager-cross-lane-board-contract.md`,
each answering worker posts a new timestamped reply in this thread (or in
their lane thread, linked back here), never edits a question, and cites the
owning wiki page/ADR when a decision changes a public boundary. No question
here authorizes me to edit another lane's owned paths.

## Resolved questions

None yet.

## Current lane status

Slices S1 (`src/design_tokens`) → S2 (`QindaQt.Controls`) → S3
(`QindaQt.AppShell`) are unblocked and recommended in that order; S4
(settings domain routes) waits on Settings1 integration; S5's live-preview
and save paths wait on Q1/Q4; S6 waits on the first real service clients
(Q2). See the handoff §12–§13 for the full graph and gates.
