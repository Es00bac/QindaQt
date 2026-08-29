# Vera Kline combined Integration QA midpoint and ancestry correction

- Timestamp: 2026-08-28T19:00:04Z
- Exact QA HEAD: `631fa4404fdee1d22a3bfe7ed12b436ea9b6b2b1`
- State: strict combined build active at 931/1285 remaining actions

The Program Manager's fresh `git fetch --prune origin` corrects the ancestry
description: current public `origin/main` `146fc483` is the direct ancestor of
QA HEAD `631fa440`; merge-base is `146fc483`, the ancestor check exits 0, and
left/right is `0/17`. This is the current additive integration tree, not a
divergent branch. The five manager-only dirty paths remain outside exact HEAD
and will not be counted as committed-HEAD evidence.

Fresh Debug cache evidence is shared libraries, tests, shell, strict warnings,
and AUTOMOC path-prefix ON; KWin plugin, production shell, and host-uinput are
OFF. The cleanly regenerated external build passed its former generated-moc
failure and is now at 931/1285 remaining actions after preserving 106 earlier
serial actions before a conservative two-job resume. No warning or source
failure has appeared after the build-only correction.

No CTest, docs, or static result is claimed yet. Remaining gates are complete
combined build; registered-test inventory and broadest safe CTest without host
input/live-host authority; exact Appearance/Settings/package regressions;
docs/link CTest; source shape; docs validator; strict MkDocs; JSON/YAML,
temporary-marker, whitespace, exact diff/status/provenance, and disk-usage
evidence. Product/source/index and host display/input/session state remain
untouched.
