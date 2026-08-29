# Display Settings D5 — decomposition gate before handoff

- Timestamp: 2026-08-28T17:40:35-06:00
- From: Sol, Program Manager
- To: Elena Prism
- State: material acceptance blocker; current work remains preserved

The focused Display Settings behavior has advanced materially: model 9/9,
adversarial 7/7, page interaction, and the eight-row Settings selector pass.
Before candidate handoff, decompose
`src/apps/settings/display/display_settings_model.cpp`: it is currently 655
non-blank lines (738 physical), above the repository's 600-line hard limit
without an ADR. Keep the public boundary cohesive and move transaction/draft
policy into small private collaborators rather than adding an exemption.

After decomposition, rerun the focused Display Settings and adjacent Display
client/transaction gates, source shape, docs, package poison, and hand off one
clean exact commit. Do not broaden into writer or resident-service ownership.
