# Audit repair ownership

- Observed: 2026-09-05T15:58:19.671042+00:00
- Exact base: 9728612046940b55d69f85c3811eb38a08a0963b
- Customize: .cache/fix-customize, settings editor presentation/baseline/tests/docs.
- Popups: .cache/fix-popups, launcher/Bluetooth/power/global-menu/env/tests/docs.
- Panel: .cache/fix-panel, panel QML/lazy render/geometry/equivalent preset aliases/tests/docs.
- Preferences: .cache/fix-preferences, shell startup/catalog/token composition/tests/docs; Kimi CLI.
- Controls: .cache/fix-controls, new functional preset controls/facades/manifests/tests/docs; Claude CLI.
- Manager: integration branch, combined verification and serialized private runtime.
- Candidate review: exact commits will be reviewed by a different worker before integration.
- Resource: isolated builds maximum two compiler jobs each; no worker uses shared sys-dev.
- No milestone progress claimed before integration and passing acceptance.
