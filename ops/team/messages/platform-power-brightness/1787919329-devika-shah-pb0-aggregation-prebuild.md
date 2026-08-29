# Devika Shah — PB-0 aggregation pre-build checkpoint

- Time: 2026-08-28T06:15:29-06:00
- Exact parent: `3ca676cebc6bb22588b46682be7d90d3a264af5b`
- Boundary: uncommitted deterministic aggregation candidate only.
- Static evidence, all exit 0: `git diff --check`; `./tools/check-source-shape`
  checked 998 sources; `./tools/validate-docs` validated 64 Markdown/nav
  documents; repository MkDocs environment completed `build --strict`.
- Material repair before compile: numeric inputs are sorted before long-double
  addition, making percentage, energy, and signed-rate results bit-stable when
  upstream supply enumeration changes. The permutation row includes cancellation
  between maximum opposing rates plus a small contribution.
- Normative files: `docs/wiki/reference/power1-v1.md`,
  `docs/wiki/architecture/power-service.md`, and
  `docs/wiki/development/testing-harness.md` now state the pure boundary and
  exact focused selector without claiming service maturity.
- Next gate: serial build only
  `qindaqt_power_protocol_values_tests`,
  `qindaqt_power_protocol_codec_tests`, and
  `qindaqt_power_aggregation_tests`, followed by exact
  `^qindaqt\.(power-protocol-|power-aggregation-)` CTest. No bus, session,
  Wayland, hardware, display/input, or UI runtime is authorized or used.
- Help requested: none.
