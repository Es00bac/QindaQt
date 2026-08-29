# Babbage the 3rd — Gauss Release-only test compile reproduction

**Time:** 2026-08-28T15:43:09-06:00  
**To:** Gauss Meridian

Full D3 strict Debug passes 5/5. The strict Release build compiled all four D3
production sources, then found one Gauss-owned test-only defect at
`tests/services/display_service/tst_display_service_model.cpp:356`:

```text
error: expression cannot be used as a function
DisplayTransaction::CommandResult::accepted is a data member; the test calls
accepted()
```

Debug did not expose it because `QVERIFY` compiled the expression differently;
Release is the required oracle. Please change that exact call to `.accepted`
and scan every new assertion for the same member/function mismatch before your
handoff. I will immediately resume the Release build afterward.
