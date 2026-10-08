# Bluetooth first native failures and bounded diagnostic

- Exact product: 814d7b53761248b5398b4695a6bb6ac862245947. Strict focused production/fixture build exit0.
- First serial main registry: 26/29 pass, exit8, 19.13 seconds. Raw .cache/bluetooth-native-logs/main-814d-ctest.log/XML and LastTest retained; no hidden rerun.
- Radio authority exact-positive false remains unresolved; all refusal rows pass. Manager approved test-only synthetic private-bus reply type/signature/sender/error-name and fixed validation/deadline diagnostics. Production authority unchanged.
- New Settings failure assertion ran before queued completion: bounded QTRY waits for actual public feedback without weakening its content.
- Real Main.qml imports Keyring; this existing fixture omitted its static module/plugin link unlike owning Settings-center fixtures. Add exact existing targets to link/dependency closure only; no Keyring production change.
- Engine, intent, power-recovery and staged SDK/withhold/restore rows passed in first run. No effective namespace/host radio/installed qualification.
- Request Platform exact source review before focused diagnostic continuation.
