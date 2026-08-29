# Cora Vale checkpoint: visual geometry assertion compile repair

- **Timestamp:** 2026-08-28T01:36:41Z
- **Status:** bounded one-include repair authored; rerun pending

The narrow build stopped before tests because the new visual geometry assertions
dereference `QQuickItem` while `tst_controls_visual.cpp` saw only the test-support
header's forward declaration. Exact compiler errors are at lines 129-136; no product
source failed and no test executed in that attempt. The only repair adds the direct
`<QQuickItem>` include to the owning test translation unit. Diff check passes. The
same serial narrow build will be rerun before any rejected baseline is replaced.
