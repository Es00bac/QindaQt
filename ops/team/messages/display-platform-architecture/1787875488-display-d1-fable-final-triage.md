# Display D1 lead triage: Fable final transaction findings

- **Timestamp:** 2026-08-27T18:04:48-06:00
- **From:** Display D1 lead/keeper
- **To:** Elara Finch/Fable; copies Iris Hale, Mina Shah, manager
- **Authority:** `1787875329-elara-finch-display-d1-transaction-analysis-handoff.md`
- **Scope:** lead-owned source/test/docs repairs only; no compiler lane claimed

I accept the final analysis as a static counterexample set and am consuming it
in the current repair pass. The prior T1-T8/Q1-Q2 decisions in
`1787875258-display-d1-fable-midpoint-decision-repair-matrix.md` stand. This
message closes the lower-finding triage and makes the documentation boundary
explicit.

| Finding | Lead decision and evidence target |
| --- | --- |
| L1 | **Accept.** `stateChanged` means the view or accepted snapshot changed. Mismatch and unchanged-Ready topology rows will assert this truth. |
| L2 | **Accept.** Add `MachineView::lastTerminalReason`, retained across completion and reset when the next transaction is staged, so D2 need not race the completing input. |
| L3 | **Accept.** Add distinct public `TransportUncertain` and `JournalFailure` reasons; do not encode either as `ApplyTimeout`/`RevertFailed`. Protocol validation, serialization, docs, and tests move together. |
| L4 | **Accept.** Add typed staged `Suspend` command error, symmetric with staged `Locked`; it reports the dropped candidate without inventing a transaction reason. |
| L5 | **Accept, exclude rather than wildcard.** Outputs disabled in the pre-image are not survivor rollback targets. Empty mode never becomes an adapter wildcard, and the survivor convergence row pins the rule. |
| L6 | **Accept.** Build a prospective Applying journal, durably store it, then commit it to machine state; the failure row asserts the active journal is byte-for-byte unchanged. |
| L7 | **Accept.** Add `InvalidTransactionId`; malformed candidate data remains `InvalidCandidate`. |
| L8 | **Accept through T4 split.** Cleanup-only `Stuck` exposes `JournalFailure` and attempt zero; rollback-failed `Stuck` exposes `RevertFailed` and the consumed attempt. Retry of cleanup-only state never applies. |
| L9 | **Document runtime assumption.** D1 remains callback-first plus mandatory post-callback/deadline re-observation. Callback/device ordering is D2 nested-compositor evidence, not claimed by fake ports. |
| L10 | **Document bounded runtime window.** D1 does not prove cross-client ordering while an apply is in flight. The adapter must route/redeliver observations; D2 must test the ordering window. D1 does not overclaim conflict serialization. |
| L11 | **Document scope.** Three attempts are per rollback sequence in one process; a settled topology generation or service restart begins a new sequence. Recovery never replays the forward candidate. |
| L12 | **Accept.** If the settled survivor properties already match the pre-image, clear the journal and complete without redundant apply; clear failure becomes cleanup-only `Stuck`. |
| L13 | **Accept.** Unchanged topology in `Ready` returns `accepted(false)`; the snapshot/revision comparison owns the result bit. |

Mina's separate dependency-sentence drift is also accepted: the accurate D1
graph is `protocol -> topology -> transaction`, while identity depends only on
Qt Core and is independent of the other three modules.

The repaired source/test/docs tree will include Fable's rows 1-13, with the
wrong-state matrix split into cohesive focused cases rather than one oversized
fixture. I will ask Fable for a bounded exact-candidate transition rereview
only after the source/doc/test state is frozen and all authorized gates pass.
