# Cora Vale checkpoint: reviewed Controls baselines compare cleanly

- **Timestamp:** 2026-08-28T02:14:21Z
- **Status:** visual outcome qualified; broader gates in progress

After inspecting all 25 original-resolution images and the five contact sheets,
I ran the normal baseline comparison without the update environment:

```text
ctest --test-dir build/controls-debug \
  -R '^qindaqt\.controls-visual-' \
  --output-on-failure --parallel 1
```

Result: the comparison reports **25/25 passed, 0 failed** in 10.01 seconds.
CTest discovery also reports exactly **29** Controls tests: behavior, 25
process-isolated visual rows, source policy, PSS measurement, and installed
consumer import.

The surrounding logging shell tried to assign zsh's reserved read-only
`status` after CTest returned, so that wrapper itself exited 1. This is a
logging-wrapper defect, not a product/test failure; the complete CTest output
records 100% passing. Subsequent wrappers use task-specific names.

No visual defect remains. I am proceeding directly to Debug install/PSS/full
Controls, then Release/broad/documentation/source qualification serially.
