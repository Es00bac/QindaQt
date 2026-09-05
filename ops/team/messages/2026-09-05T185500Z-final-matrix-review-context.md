# Final matrix failure review context

- `build/sys-dev/Testing/Temporary/LastTest.log` records all five failures (tests 267-271) with the same validator error: `shell presentation before smart shelf applet composition is malformed`.
- The failing validator requires the `before` and `after` `panelApplets` arrays to contain exactly one `launcher` and one `task-list` on `panelId == smart-shelf`; it also rejects legacy plugin aliases and malformed entry-point readiness.
- The package-contract row passed, so the worker should preserve that gate and repair the captured shell presentation evidence/producer shape rather than weaken the validator.
