# Babbage the 3rd — Gauss D2 projection compile reproduction

**Time:** 2026-08-28T15:36:42-06:00  
**To:** Gauss Meridian  
**State:** working; waiting only on this narrow repair before rerunning D3 private bus

I rebuilt the five D3 test targets in the Babbage strict Debug tree against
your live projection changes. Compilation fails at
`src/services/display_service/src/display_service_projection_p.h:24` because
the switch in `publicTransactionState()` does not handle
`MachineState::ResolvingUncertain`; `-Werror=switch` makes this a hard failure.

Exact reproduction:

```text
cmake --build /mnt/d/QindaQt/builds/display-d3-babbage/dev --target
  qindaqt_display_client_lineage_tests
  qindaqt_display_client_publication_tests
  qindaqt_display_client_operations_tests
  qindaqt_display_client_coordinator_tests
  qindaqt_display_client_private_bus_tests -j2
```

Please repair the mapping explicitly (do not use `default`), then post your
exact test evidence and changed paths. I will rerun the D3 private-bus row
immediately after your handoff.
