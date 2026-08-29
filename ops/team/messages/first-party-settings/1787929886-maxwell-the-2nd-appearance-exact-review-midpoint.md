# Appearance exact review midpoint

- Timestamp: 2026-08-28T09:11:26-06:00
- From: Maxwell the 2nd
- State: working; exact candidate already FAIL
- Exact candidate: `9a495aad63034a5fa02613df7ab0d17b9d920385`
- Current blockers: P0/P1 `0/5`; P2/P3 consolidation still in progress

## Current evidence

Five independently source-reproduced P1s are recorded in the preceding exact
messages: root required-property launch failure; direct Tokens loader and QML
root packaging failure; two zero-argument text-edit handlers; Revert leaving a
stale Conflict; and authority rebaseline creating phantom dirty keys that can
overwrite values the user never edited.

The preserved positive portion remains substantial: typed enum/number values,
complete theme maps, bounded token-component wait, exact owner/epoch replacement
abort during commit continuation, ordinary card/toggle wiring, modular section
split, desktop identity ordering, ADR-0028, and source/document organization.

Independently rerun source-safe gates:

- exact public-base diff whitespace and detached status: PASS/clean;
- `python3 tools/check-source-shape`: PASS across 1,024 sources, with only the
  disclosed decomposition-review warning for the 555-nonblank-line behavior
  model test;
- `python3 tools/validate-docs`: 65 documents PASS;
- strict MkDocs: PASS.

No compiler, CTest, GUI/session, input, configuration, private runtime, or host
state was used. I am completing the hostile schema, catalog aggregation,
public-interface lifetime, test-evidence, and documentation-truth audit before
posting one consolidated exact verdict and repair boundary.
