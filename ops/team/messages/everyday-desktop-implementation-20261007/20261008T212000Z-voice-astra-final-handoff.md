# Secure Voice credentials: final accepted candidate handoff

- Desktop candidate: `e86b77f667a1beb8a9f4f116e246fda92c2d1d38`, base `32aae08d89a1f4ebdc28481866c42c65f7875188`.
- Gabbee candidate: `b4462610de605490336b0c1e702f15b7a45f4b4b`, base `78ba261d1f6c515d8a277bab202ed702b762b7d1`.
- Both branches: `worker/everyday-voice-credentials-20261008`, preserved directly in the qinda bare hubs. Existing isolated worktrees remain intact.
- Independent exact-pair source and bounded actual-evidence ACCEPT: reviewer commit `c819cd6e3`, receipt `20261008T211908Z-astra-voice-final-banner-pair-accept.md`.

## Outcome and changed paths

Settings adds masked ElevenLabs entry, explicit saved-key reload, immediate draft clearing, provider-neutral feedback and configured/effective fallback truth in the main card and credential section. Voice Off withdraws the configuration client. Older providers remain usable. Optional same-owner VoiceConfiguration1 uses actual native sender/serial and owner-generation authority; Voice1 and Settings1 schemas are unchanged.

Desktop ownership: `src/services/voice_configuration/**`, `src/apps/settings/voice/**`, corresponding focused tests, minimal `src/CMakeLists.txt`/`tests/CMakeLists.txt`, Voice/Settings/module-boundary wiki, ADR0364/nav/index and own coordination records. Provider ownership: `qindaqt_voice_configuration*`, secure store, minimal controller/main_service/Voice1 busy hooks, pyproject dependency, focused tests and README. Exact full path manifests are frozen in the evidence pack below.

The headless provider owns Secret Service persistence and reload. Generator search is bounded at two items; startup neither prompts nor creates a collection. Explicit Save/Reload can invoke native unlock. Authentication and Hello are each bounded to one second; sends, receives and prompts share a fifteen-second operation deadline. One worker/capture reservation survives UI timeout and owner loss until actual settlement. Copied delayed messages and GUI-thread idle/generation checks prevent stale reply/engine mutation. Failed reload preserves the existing engine; environment-key precedence stays visible.

## Acceptance evidence

- Strict normal Debug build (KWin plugin OFF, strict warnings ON), targets `qindaqt_voice_configuration_tests`, `qindaqt_voice_configuration_transport_tests`, `qindaqt_settings_voice_tests`, `qindaqt_settings_voice_credentials_tests`: exit 0.
- `QINDAQT_GABBEE_SOURCE=<provider-worktree> ctest --test-dir build/voice-native --output-on-failure -R '^qindaqt\.(voice-configuration|settings-voice).*'`: 4/4 pass, 40 Qt checks, zero failures/skips. Includes actual Python-provider/C++-client snapshot/reload/save, genuine/foreign captured-serial reply controls, and delete/stop during owner/reply emissions.
- Full compact 360x760 VoicePage compiled/disk at 1x and 2x with real published tokens: pass, no QML warnings. Main card explicitly reports the actual local fallback. Root inspected the captures.
- Owning QML install stage: exact qmldir-relative source hashes match; removing `prefer` permits disk 1x/2x loads; withholding VoiceCredentialSection.qml produces required failure exit 1. Public configuration headers/static archive staged.
- Private-HOME, invalid-live-bus pytest over Voice1, controller, config, offline fallback and all new configuration tests: 98 passed plus 2 subtests, 1 existing GLib deprecation warning, 1.36s, exit 0. Earlier independent unittest run passed 30 new cases and 22 controller/config cases. Actual PyQt disposable-bus ABI smoke passed.
- `mkdocs build --strict --site-dir build/voice-docs` and `python3 tools/validate-docs`: exit 0; 535 Markdown documents/nav validated.
- All native work used the manager lease and resource limits; it is released. No microphone, real key read/write or paid API call was used in author verification.

## Immutable raw pack

Qinda worktree path: `build/voice-evidence-e86b77f66-b446261-20261008T211924Z.tar.gz`.
SHA256: `f73286a1cbb84676af588135d3d8dd5a368a946815ae8b1bc562d0629a20c618`.
The matching read-only directory contains `INDEX.md`, `SHA256SUMS`, final LastTest/Ninja/CMake cache, exact source/changed-path manifests, all available failed and passing Voice logs, and empty-key synthetic page captures. Earlier compile/fixture failures are retained honestly; final banner acceptance supersedes earlier mutable runs.

## Requested manager action and remaining limits

Integrate the exact accepted pair, merge minimal shared registry changes with current manager main, promote ADR0364 as appropriate, and package the combined Files/Voice desktop release with Gabbee r6. Gabbee directly requires `dev-python/jeepney >= 0.9`; retain its existing SecretStorage >=3.3 requirement. Desktop adds a private `dbus-1` link dependency already present in the project. Root owns release recipes/Portage and installed native prompt/recovery/adoption. No installed-user recovery, authentication success or real dictation is claimed by this handoff.

Previous inactive worker dirty board/message files were preserved untouched and excluded from these candidate commits. No additional product change or test overwrite is pending. Available to help with bounded package-image/installed-route diagnosis if the manager routes an exact reproduction.
