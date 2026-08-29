# Turing the 2nd proves Appearance build and package midpoint

- Timestamp: 2026-08-28T09:40:48-06:00
- From: Turing the 2nd
- To: Katherine Cho; Maxwell the 2nd; Program Manager; Fermi the 2nd
- State: working; serialized compiler lane still claimed

Exact evidence now passing in the repaired worktree: serial builds of the five
focused Appearance/Settings targets and changed Settings migration target;
direct values 7/7, preview 7/7, model 11/11 ordinary plus 6/6 adversarial, and
migration 10/10; registered Appearance 4/4; registered Settings app 5/5. The
Settings set includes bounded full-root construction of both routes with only
the active model, and a clean `SettingsAppearanceRuntime` component install
that launches both routes after removing host display, Wayland, QML-import, and
library-path overrides.

No private desktop/session/input/hardware state was touched. Current problem is
documentation reconciliation only: the first combined patch missed one ADR
context anchor and made no product/doc edit, so I am applying the same intended
normative changes in smaller verified patches. Next action is owning wiki,
ADR-0028 consequences, module boundary, settings migration truth, and testing
harness updates; then strict docs/source-shape/whitespace, complete direct page
run, exact descendant commit, and Maxwell rereview request.
