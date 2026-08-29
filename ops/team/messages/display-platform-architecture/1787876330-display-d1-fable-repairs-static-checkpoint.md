# Display D1 checkpoint: Fable repairs consumed, source/tests/docs aligned

- **Timestamp:** 2026-08-27T18:18:50-06:00
- **From:** Display D1 lead/keeper
- **To:** manager; copies Elara Finch/Fable, Iris Hale, Mina Shah
- **Base/worktree:** `94e84077e33a279dcebee24511e7dbdf1b87e3e1`,
  `/home/cabewse/work_SPaC3/container-wm-workers/display-d1`
- **State:** uncommitted implementation checkpoint; no build/runtime/acceptance
  claim and no compiler use

The accepted T1-T8/Q1-Q2 and L1-L13 findings are now reflected together in
the lead-owned transaction source, focused fake-clock/port tests, public port
contract, `display-service.md`, `display1-v1.md`, ADR-0015, and the testing
harness. In particular:

- repeated rollback inputs cannot reset the three-attempt sequence;
- settling defers cancel/lock/suspend and external abandon until the explicit
  settled snapshot; a restored original set uses the full pre-image;
- confirmation-clear failure stays awaiting confirmation, while known-safe
  terminal clear failures become cleanup-only `Stuck(JournalFailure)` whose
  retry cannot apply;
- ordinary uncertainty mismatch waits then rolls back; same-set recovery
  matching neither endpoint accepts external truth without fighting it;
- changed sets received through observation route to topology, and `Stuck`
  adopts current topology before retry;
- live snapshot projection is separate from strict candidate legality;
- post-callback/deadline re-observation, callback-first D1 ordering, and the
  D2-only cross-client/runtime evidence window are explicit;
- terminal reason, result-bit truth, transport/journal reason vocabulary,
  invalid-ID/staged-suspend errors, disabled pre-image survivors, and
  prospective journal gating are public and tested.

Two focused selectors were added inside the owned transaction test module:
`qindaqt.display-transaction-adversarial` covers the repaired counterexamples,
and `qindaqt.display-transaction-invalid-ordering` drives all twelve states and
asserts wrong inputs preserve view, snapshot, journal, and port effects.

Mina Shah's only concrete drift in
`1787876100-mina-shah-docs-trace-handoff.md` is accepted and consumed:
`module-boundaries.md` now states the actual
`protocol -> topology -> transaction` chain, with identity depending only on
Qt Core and independent of the other three modules.

Static evidence at this checkpoint:

- `./tools/validate-docs` — exit 0, 51 Markdown documents/navigation valid;
- `./tools/check-source-shape --largest 30` — exit 0, 885 files checked;
  largest owned production file is 436 non-blank lines;
- `git diff --check` — exit 0;
- forbidden include/symbol/dependency scans — no platform/runtime dependency;
  Qt DBus appears only in `display_protocol` serialization.

`mkdocs build --strict` remains unavailable in the current shell
(`mkdocs: command not found`). Configure/build/test/compiler gates have not
been attempted because the global compiler lane has not been released. The
next action is manager assignment of that lane; after all Debug/Release/
sanitizer/broad/docs gates pass, I will freeze an exact candidate and request
independent exact-commit review.
