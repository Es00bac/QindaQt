# SPDX-License-Identifier: GPL-3.0-or-later
find_package(Qt6 REQUIRED COMPONENTS Gui Qml Quick QuickControls2 Test)
find_package(Wayland REQUIRED COMPONENTS Client)
find_package(LayerShellQt 6.6.6 REQUIRED)
set(prompt_dir "${PROJECT_SOURCE_DIR}/src/services/keyring/prompt")
qt_add_executable(qindaqt_keyring_ui_prompt
    "${prompt_dir}/main.cpp" "${prompt_dir}/prompt_controller.cpp"
    "${prompt_dir}/prompt_controller.h" prompt_input.cpp)
set_source_files_properties("${prompt_dir}/Main.qml" PROPERTIES QT_RESOURCE_ALIAS Main.qml)
qt_add_qml_module(qindaqt_keyring_ui_prompt URI QindaQt.KeyringPrompt VERSION 1.0
    RESOURCE_PREFIX /qt/qml OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/prompt-qml"
    QML_FILES "${prompt_dir}/Main.qml")
target_link_libraries(qindaqt_keyring_ui_prompt PRIVATE qindaqt_keyring_service
    QindaQt::AuthenticationOverlay QindaTK::qindatk Qt6::Qml Qt6::Quick
    Qt6::QuickControls2 Qt6::Test Wayland::Client)
qindaqt_enable_warnings(qindaqt_keyring_ui_prompt)
qt_add_executable(qindaqt_keyring_clipboard_probe clipboard_probe.cpp)
target_link_libraries(qindaqt_keyring_clipboard_probe PRIVATE Qt6::Gui Qt6::Quick Qt6::Core)
qindaqt_enable_warnings(qindaqt_keyring_clipboard_probe)
qt_add_executable(tst_keyring_native_ui tst_native_ui.cpp)
target_include_directories(tst_keyring_native_ui PRIVATE "${PROJECT_SOURCE_DIR}")
target_link_libraries(tst_keyring_native_ui PRIVATE qindaqt_settings_keyring_qml
    qindaqt_settings_keyring_qmlplugin QindaQt::SettingsKeyring QindaQt::KeyringStorage QindaQt::KeyringProtocol
    QindaQt::CompositorNames QindaQt::TokensQml qindaqt_themes Qt6::Test Qt6::DBus Qt6::Quick Qt6::Qml Qt6::Gui)
qindaqt_enable_warnings(tst_keyring_native_ui)
add_test(NAME keyring_native_ui_journey COMMAND ${Python3_EXECUTABLE}
    "${CMAKE_CURRENT_SOURCE_DIR}/run_journey.py" "$<TARGET_FILE:tst_keyring_native_ui>"
    "$<TARGET_FILE:qindaqt-keyring>" "$<TARGET_FILE:qindaqt-settings-service>"
    "$<TARGET_FILE:qindaqt_keyring_ui_prompt>" "$<TARGET_FILE:qindaqt_keyring_clipboard_probe>"
    "${QINDAQT_KWIN_WAYLAND}" "${PROJECT_SOURCE_DIR}/data/settings")
set_tests_properties(keyring_native_ui_journey PROPERTIES TIMEOUT 180
    SKIP_RETURN_CODE 77 LABELS "keyring;wayland;private-fixture")
target_compile_definitions(tst_keyring_native_ui PRIVATE QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}" QINDAQT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml")
