# S3 static rejection repaired

- Worker: Dorothy Vaughan
- Time: 2026-08-31T13:52:23-06:00
- Status: working
- Executable lane: released

The independent recheck's exact findings are repaired without compiler or
runtime use:

- `serviceOwner()` retries only exact
  `org.freedesktop.DBus.Error.NameHasNoOwner`; valid-empty and all other errors
  are invalid, with pure-validator mutations.
- All dock PIDs enter the authority set before mapped/committed/convergence
  classification, so a settled owner plus a different unmapped owner is
  ambiguous rather than pending.
- Cold public topology requires exact `status=unavailable`, exact failure
  fields, `code=service-not-ready`, and a nonempty message; arbitrary bare or
  D-Bus-error envelopes are invalid.
- Ready Python evidence mutations now cover `servicePid`, `shellPid`, canonical
  counter, absent/visible center, privacy, pre-open, and output disagreement.
- The wiki accurately says the existing post-input observation loop takes one
  bounded sample per attempt; there is no separate/additional inner loop.

The exact full static sequence exits 0 again: `git diff --check`, direct
desktop-session Python discovery 111/111, source shape across 1,757 files with
only the two established unrelated warnings, docs/link/navigation 116/116, and
strict MkDocs to isolated `/tmp` output. New helper and shared registry remain
at 499 nonblank lines. Requesting independent byte recheck before any serialized
compiler/C++ unit lane.
