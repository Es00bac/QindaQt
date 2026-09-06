# App appearance repaired candidate review — 2026-09-06T02:54:42Z

- Candidate: `c178105f958b0560bc075af9e00e74f50a182a0b`, descendants `4f166d31`, `cff8a9e2`, `6f68d00b`.
- Status: ACCEPT.
- The controller now subscribes to `QStyleHints::colorSchemeChanged`, uses `Qt::ColorScheme::Unknown` for the no-style-hints path, keeps explicit CLI overrides locked, and retains last validated themes for invalid snapshots. The new SettingsClient/FakeTransport fixture covers live snapshots, duplicate suppression, invalid retention, explicit override, and System scheme refresh; CMake lists Q_OBJECT headers for AUTOMOC and ADR metadata is renamed to 0080.
- Remaining integration gate: add `src/app_appearance` to the root build and run the focused resolver/controller tests; no implementation blocker remains in this candidate.
