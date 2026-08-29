# Lead triage of Omar Finch consolidated audit

- Lead/keeper: Soren Pike
- Timestamp: 2026-08-27T17:42:08-06:00
- Reviewed finding: `1787873998-omar-finch-consolidated-audit.md`

I accept Omar's no-static-blocker verdict as independent read-only evidence,
not as runtime qualification. The listed runtime boundaries remain mandatory
and unclaimed until the private installed rows execute.

## Finding disposition

- **C1 consumed as a qualification defect.** A ten-repetition driver with a
  180-second budget per repetition cannot share an identical 1800-second CTest
  timeout: staging, Python startup, result serialization, and process-group
  teardown require a nonzero outer margin. I will raise only the race-10x CTest
  timeout to 2400 seconds and add a static/unit assertion so later inner-budget
  changes cannot silently erase that margin. I am deferring this product edit
  until the currently active fresh Debug build reaches a terminal boundary;
  changing its CMake input mid-build would invalidate provenance.
- **C2 accepted as a bounded fail-safe timing caveat.** The negative windows
  can create a false failure, not a false pass. Keep unchanged unless real
  nested evidence reproduces it.
- **C3 accepted as a bounded fail-safe focus-race caveat.** Keep unchanged
  unless the private rows reproduce it.
- **C4 accepted as a bounded fail-safe probe-budget caveat.** Keep unchanged
  for the first private run; increase only with measured phase timing.
- **C5 accepted as a diagnostic-quality caveat.** It does not weaken the
  uniqueness assertion; no pre-runtime repair is warranted.

Omar's exact executable traces materially reduce review ambiguity. After the
C1 repair, the focused build/test pass will be rerun for changed registration
and unit coverage before Release/sanitizer/package/nested qualification.
