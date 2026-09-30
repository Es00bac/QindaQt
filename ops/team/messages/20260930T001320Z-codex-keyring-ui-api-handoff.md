# PK4 API checkpoint (presentation may start)

2026-09-30T00:13:20Z

Exact candidate ca92bfcb5ca0225de142fdef91865268f2704a4b pushed to qinda local hub feature/native-keyring-ui. Clean merge accepted main4c0a877f (merge only task/handoff state); no installs/live services/secrets/PAM.

Changed paths:

- data/settings/schema-v2.json
- docs/wiki/adr/0302-native-passwords-keys-client-and-policy.md
- docs/wiki/adr/index.md
- docs/wiki/architecture/keyring-client.md
- docs/wiki/architecture/keyring-daemon.md
- docs/wiki/architecture/module-boundaries.md
- docs/wiki/architecture/secret-service.md
- docs/wiki/architecture/settings-service.md
- mkdocs.yml
- src/CMakeLists.txt
- src/apps/settings/keyring/CMakeLists.txt
- src/apps/settings/keyring/include/qindaqt/apps/settings_keyring/keyring_preferences.h
- src/apps/settings/keyring/include/qindaqt/apps/settings_keyring/keyring_settings_model.h
- src/apps/settings/keyring/keyring_preferences.cpp
- src/apps/settings/keyring/keyring_settings_model.cpp
- src/services/keyring/CMakeLists.txt
- src/services/keyring/daemon/item_methods.cpp
- src/services/keyring/daemon/native_item_methods.cpp
- src/services/keyring/daemon/process_prompt_provider.cpp
- src/services/keyring/daemon/process_prompt_provider.h
- src/services/keyring/daemon/prompt_password_change.cpp
- src/services/keyring/daemon/prompts.cpp
- src/services/keyring/daemon/properties.cpp
- src/services/keyring/daemon/secret_service.cpp
- src/services/keyring/daemon/secret_service.h
- src/services/keyring/daemon/wire_types.h
- src/services/keyring/data/api.xml
- src/services/keyring/prompt/Main.qml
- src/services/keyring/prompt/main.cpp
- src/services/keyring/prompt/prompt_controller.cpp
- src/services/keyring/prompt/prompt_controller.h
- src/services/keyring_client/CMakeLists.txt
- src/services/keyring_client/include/qindaqt/services/keyring_client/keyring_gateway.h
- src/services/keyring_client/include/qindaqt/services/keyring_client/qt_keyring_gateway.h
- src/services/keyring_client/keyring_reply_validation.cpp
- src/services/keyring_client/keyring_reply_validation.h
- src/services/keyring_client/qt_keyring_gateway.cpp
- src/services/keyring_protocol/CMakeLists.txt
- src/services/keyring_protocol/include/qindaqt/services/keyring_protocol/prompt_metadata.h
- src/services/keyring_protocol/include/qindaqt/services/keyring_protocol/wire_types.h
- src/services/keyring_protocol/prompt_metadata.cpp
- src/services/keyring_protocol/wire_types.cpp
- src/settings/include/qindaqt/settings/settings_types.h
- src/settings/src/settings_types.cpp
- tests/CMakeLists.txt
- tests/services/keyring/CMakeLists.txt
- tests/services/keyring/display_prompt.py
- tests/services/keyring/native_scripted_prompt.py
- tests/services/keyring/scripted_prompt.py
- tests/services/keyring/test_native_prompts.py
- tests/services/keyring/tst_prompt_metadata.cpp
- tests/services/keyring/tst_prompt_qml.cpp
- tests/services/keyring_client/CMakeLists.txt
- tests/services/keyring_client/tst_keyring_client.cpp
- tests/services/keyring_client/tst_keyring_preferences.cpp
- tests/services/keyring_client/tst_keyring_settings_model.cpp

Verification: focused daemon/prompt/protocol/client/model/preferences builds j24/l24 exit0. Six distinct QT_FATAL_WARNINGS CTest rows pass (native11, client4, model5, preferences4, QML6, codec4), client/model/preferences final3/3 rerun exit0 after owner/later-reply fence. Earlier existing20 real-client and display9 gates passed; final broad affected gate manager-owned. Strict MkDocs452 exit0, validator452 exit0, git diff --check exit0. Logs build/pk4/{checkpoint-final-gates,api-gates,api-docs}.log.

Public borrowed gateway/model/preferences headers own lifetime/thread/error contracts; no password input in Settings. Native reveal reauthenticates even unlocked; metadata index trust explicit; creator informational not attested. Owned wire copies wiped; unavoidable Qt/DBus/QML copies not universally secure. Typed schema-v2 Keyring domain appended without ordinal change. Last confirmed policy survives Settings service loss; uncertain saves never reported saved.

Next: presentation worker owns composition/QML/route wiring; this worker remains working on resident native Lock1 plus true ext-idle policy, using PF8 public readonly transport. PK4 remains incomplete until UI, clipboard deadline and resident policy executable gates pass.
