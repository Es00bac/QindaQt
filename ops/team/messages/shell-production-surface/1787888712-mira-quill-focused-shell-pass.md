# Mira Quill result: focused shell regressions pass 25/25

- **Timestamp:** 2026-08-28T03:45:12Z
- **Status:** focused acceptance passed; proportional package gate continues

The exact 25 focused shell-adjacent binaries built in **142/142 serial steps,
exit 0**. Their CTest selector then passed **25/25, exit 0, 0.95 seconds** with
`--parallel 1 --stop-on-failure`. Coverage includes:

- layout geometry/validation and applet production resolution;
- all five customization mutation/history/coordinator/query rows;
- visibility modes, scope, validation, snapshot/state, wire round trip,
  exact-owner client, and private-D-Bus Qt transport;
- surface configuration, controller liveness, and runtime planning;
- interaction, visibility inventory, runtime plan, and output-match
  orchestration;
- shell runtime options and the fail-closed Wayland protocol parser.

The exact short focus-test root
`/home/cabewse/.cache/qst-focus.E86nML` was empty after CTest and removed with
`rmdir`; `test ! -e` returned 0. Static gates also remain clean:

- `git diff --check`: exit 0;
- `tools/check-source-shape`: 831 files, zero allowlisted skips;
- `python3 tools/docs_validation.py`: 47 Markdown documents and navigation
  valid.

No `mkdocs` executable was found in PATH or the workspace environment, so the
strict MkDocs renderer remains explicitly unavailable; the stdlib repository
link/navigation gate passes. Proportional production-shell lint/staged-package
evidence remains before commit.
