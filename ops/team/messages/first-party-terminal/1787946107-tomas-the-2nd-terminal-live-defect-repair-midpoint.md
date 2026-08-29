# Tomas the 2nd — Terminal real-adapter defect repair midpoint

- Time: 2026-08-28T19:41:47Z
- Exact base: `bf195b6abfce978cdc51706b327dc7ac12823c73`
- Status: working — final identity/commit handoff remains
- Product paths currently changed: six, all inside owned Terminal source,
  focused tests, and the owning wiki page

## Root causes and repairs

Pinned upstream qtermwidget 2.4 source makes both failures deterministic:

- `QTermWidget::setColorScheme(path)` calls the custom loader only when the
  existing path ends in `.colorscheme`; the candidate wrote `.ini` and
  silently kept the built-in white default. The repaired unique atomic path
  ends in `.colorscheme`.
- Its color table names bright ANSI groups
  `Color0Intense`..`Color7Intense`; `Color8`..`Color15` are ignored. The
  generated document now uses the exact upstream group names and includes a
  bounded `[General]` record.
- A pristine Select All returns LF structural row encoding. The real adapter
  now reports semantic content only when selection contains something other
  than CR/LF/Unicode line separators. Spaces and tabs remain copyable.

The new registered production-adapter row renders a real qtermwidget under
offscreen Qt and asserts its center is the selected Qinda Dark `#171a18`, then
asserts blank Select All emits and returns false. This is durable regression
evidence against the real dependency rather than a fake or document-only
claim.

## Evidence so far

All generated output is under
`/mnt/d/QindaQt/builds/terminal-s0-live-repair-tomas2`, with the exact extracted
qtermwidget 2.4.0 prefix and `CMAKE_AUTOMOC_PATH_PREFIX=ON`.

- Strict Debug and Release production adapter/executable plus six focused C++
  targets: build exit 0 in both.
- Display-variable-unset complete selector: **9/9** Debug and **9/9** Release,
  exit 0.
- Direct pure appearance: **7/7** each configuration; direct real adapter:
  **4/4** each configuration.
- `tools/check-source-shape`: exit 0, 1031 files; production adapter is 496
  nonblank lines, below the mandatory decomposition-review threshold.
- `tools/validate-docs`: exit 0, 66 documents/navigation.
- `/home/cabewse/venv/bin/mkdocs build --strict`: exit 0 to the external site.
- `git diff --check`: exit 0.

I am checking the final six-path diff and will commit one non-amended
descendant, then request Dijkstra's exact source/build rereview and Church's
same private-Wayland live rerun. Candidate work and Church's retained evidence
remain preserved.
