# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 REQUIRED COMPONENTS QuickTest)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_library(
    qindaqt_controls_test_support STATIC
    control_test_support.cpp
    control_test_support.h
)
target_link_libraries(
    qindaqt_controls_test_support
    PUBLIC
        QindaQt::TokensQml
        qindaqt_themes
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
)
target_compile_definitions(
    qindaqt_controls_test_support
    PUBLIC
        QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
        QINDAQT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
        QINDAQT_CONTROLS_TEST_QML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml"
        QINDAQT_CONTROLS_FONT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fonts"
        QINDAQT_CONTROLS_PINNED_THEME_DIR="${CMAKE_CURRENT_BINARY_DIR}/pinned-themes"
)
qindaqt_enable_warnings(qindaqt_controls_test_support)
add_dependencies(
    qindaqt_controls_test_support
    qindaqt_tokens_qmlplugin
    qindaqt_controls_qmlplugin
)

qt_add_executable(
    qindaqt_controls_behavior_tests
    tst_controls_behavior.cpp
    state_card_accessibility_test.cpp
    state_card_accessibility_test.h
)
target_link_libraries(
    qindaqt_controls_behavior_tests
    PRIVATE qindaqt_controls_test_support Qt6::QuickControls2 Qt6::Test
)
qindaqt_enable_warnings(qindaqt_controls_behavior_tests)
add_test(NAME qindaqt.controls-behavior COMMAND qindaqt_controls_behavior_tests)
set_tests_properties(
    qindaqt.controls-behavior
    PROPERTIES
        LABELS "controls;offscreen;accessibility"
        ENVIRONMENT
            "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1"
)

qt_add_executable(qindaqt_controls_visual_tests tst_controls_visual.cpp)
target_link_libraries(
    qindaqt_controls_visual_tests
    PRIVATE qindaqt_controls_test_support Qt6::QuickControls2 Qt6::Test
)
target_compile_definitions(
    qindaqt_controls_visual_tests
    PRIVATE QINDAQT_CONTROLS_BASELINE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/baselines"
)
qindaqt_enable_warnings(qindaqt_controls_visual_tests)

function(qindaqt_add_controls_visual_test scale row suffix)
    math(EXPR whole "${scale} / 100")
    math(EXPR fraction "${scale} % 100")
    set(scale_value "${whole}.${fraction}")
    add_test(
        NAME qindaqt.controls-visual-${scale}-${suffix}
        COMMAND
            "${CMAKE_COMMAND}"
            "-DQINDAQT_VISUAL_TEST=$<TARGET_FILE:qindaqt_controls_visual_tests>"
            "-DQINDAQT_SCALE=${scale}"
            "-DQINDAQT_ROW=${row}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_controls_visual_row.cmake"
    )
    set_tests_properties(
        qindaqt.controls-visual-${scale}-${suffix}
        PROPERTIES
            LABELS "controls;visual"
            ENVIRONMENT
                "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_SCALE_FACTOR=${scale_value};QT_SCALE_FACTOR_ROUNDING_POLICY=PassThrough;QINDAQT_CONTROLS_TEST_SCALE=${scale_value}"
    )
endfunction()

set(qindaqt_controls_visual_themes
    qinda-light
    qinda-dusk
    qinda-dark
    qinda-high-contrast
    qinda-macos
)
foreach(theme IN LISTS qindaqt_controls_visual_themes)
    foreach(profile IN ITEMS compact ordinary large)
        qindaqt_add_controls_visual_test(100 "${theme}-${profile}" "${theme}-${profile}")
    endforeach()
    foreach(scale IN ITEMS 125 150)
        qindaqt_add_controls_visual_test(${scale} "${theme}" "${theme}-ordinary")
    endforeach()
endforeach()

# AGENT-CONTRACT: This row is the host-Noto-hidden canary for the byte-pinned
# fixture. It reruns the theme row that previously bypassed the registered
# families (qinda-high-contrast names "Noto Sans"/"Noto Sans Mono" in the
# product catalog) under the checked-in fontconfig that rejects exactly those
# host families while keeping every other documented host font and rendering
# rule. A fixture that renders any host Noto byte drifts against the reviewed
# baseline here and fails. XDG_CACHE_HOME keeps fontconfig's cache inside the
# build tree; do not remove it or the row writes to the user's cache.
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/fontconfig-no-noto-cache")
add_test(
    NAME qindaqt.controls-visual-no-noto-100-qinda-high-contrast-compact
    COMMAND
        "${CMAKE_COMMAND}"
        "-DQINDAQT_VISUAL_TEST=$<TARGET_FILE:qindaqt_controls_visual_tests>"
        "-DQINDAQT_SCALE=100"
        "-DQINDAQT_ROW=qinda-high-contrast-compact"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_controls_visual_row.cmake"
)
set_tests_properties(
    qindaqt.controls-visual-no-noto-100-qinda-high-contrast-compact
    PROPERTIES
        LABELS "controls;visual;fonts"
        ENVIRONMENT
            "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_SCALE_FACTOR=1.0;QT_SCALE_FACTOR_ROUNDING_POLICY=PassThrough;QINDAQT_CONTROLS_TEST_SCALE=1.0;FONTCONFIG_FILE=${CMAKE_CURRENT_SOURCE_DIR}/fontconfig/no-noto/fonts.conf;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/fontconfig-no-noto-cache"
)

qt_add_executable(qindaqt_controls_font_pinning_tests tst_controls_font_pinning.cpp)
target_link_libraries(
    qindaqt_controls_font_pinning_tests
    PRIVATE qindaqt_controls_test_support Qt6::Gui Qt6::Test
)
qindaqt_enable_warnings(qindaqt_controls_font_pinning_tests)
add_test(NAME qindaqt.controls-font-pinning COMMAND qindaqt_controls_font_pinning_tests)
set_tests_properties(
    qindaqt.controls-font-pinning
    PROPERTIES LABELS "controls;fonts" ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

qt_add_executable(
    qindaqt_controls_font_fixture_negative_tests
    tst_controls_font_fixture_negative.cpp
)
target_link_libraries(
    qindaqt_controls_font_fixture_negative_tests
    PRIVATE qindaqt_controls_test_support Qt6::Gui
)
qindaqt_enable_warnings(qindaqt_controls_font_fixture_negative_tests)

foreach(mode IN ITEMS missing corrupt wrongfamily)
    add_test(
        NAME qindaqt.controls-font-fixture-${mode}
        COMMAND
            "${CMAKE_COMMAND}"
            "-DQINDAQT_NEGATIVE_EXECUTABLE=$<TARGET_FILE:qindaqt_controls_font_fixture_negative_tests>"
            "-DQINDAQT_MODE=${mode}"
            "-DQINDAQT_FONT_DIR=${CMAKE_CURRENT_SOURCE_DIR}/fonts"
            "-DQINDAQT_RUNTIME_DIR=${CMAKE_CURRENT_BINARY_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_controls_font_fixture_negative.cmake"
    )
    set_tests_properties(
        qindaqt.controls-font-fixture-${mode}
        PROPERTIES LABELS "controls;fonts" ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endforeach()

add_test(
    NAME qindaqt.controls-source-policy
    COMMAND
        "${CMAKE_COMMAND}"
        -DQINDAQT_CONTROLS_SOURCE_DIR=${PROJECT_SOURCE_DIR}/src/controls/qml
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_control_source_policy.cmake"
)
set_tests_properties(qindaqt.controls-source-policy PROPERTIES LABELS "controls;policy")

qt_add_executable(qindaqt_controls_bare_memory_probe bare_memory_probe.cpp)
target_link_libraries(qindaqt_controls_bare_memory_probe PRIVATE Qt6::Gui Qt6::Qml Qt6::Quick)
qindaqt_enable_warnings(qindaqt_controls_bare_memory_probe)

qt_add_executable(qindaqt_controls_memory_probe controls_memory_probe.cpp)
target_link_libraries(
    qindaqt_controls_memory_probe
    PRIVATE qindaqt_controls_test_support Qt6::QuickControls2
)
qindaqt_enable_warnings(qindaqt_controls_memory_probe)

add_test(
    NAME qindaqt.controls-pss-measurement
    COMMAND
        "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/measure_pss.py"
        --bare $<TARGET_FILE:qindaqt_controls_bare_memory_probe>
        --controls $<TARGET_FILE:qindaqt_controls_memory_probe>
)
set_tests_properties(
    qindaqt.controls-pss-measurement
    PROPERTIES LABELS "controls;benchmark" TIMEOUT 30 RUN_SERIAL TRUE
)

add_test(
    NAME qindaqt.controls-installed-import
    COMMAND
        "${CMAKE_COMMAND}"
        "-DQINDAQT_CMAKE=${CMAKE_COMMAND}"
        "-DQINDAQT_BUILD_DIRECTORY=${CMAKE_BINARY_DIR}"
        "-DQINDAQT_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/installed-controls-stage"
        "-DQINDAQT_QML_INSTALL_DIR=${QT6_INSTALL_QML}"
        "-DQINDAQT_QMLTESTRUNNER=$<TARGET_FILE:Qt6::qmltestrunner>"
        "-DQINDAQT_QMLLINT=$<TARGET_FILE:Qt6::qmllint>"
        "-DQINDAQT_QT_QML_IMPORT_ROOT=$<TARGET_FILE_DIR:Qt6::qmllint>/../qml"
        "-DQINDAQT_CONSUMER_QML=${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_installed_controls.qml"
        "-DQINDAQT_EXPECTED_QML_DEPLOY_PATHS=$<JOIN:$<TARGET_PROPERTY:qindaqt_controls_qml,QINDAQT_QML_DEPLOY_PATHS>,|>"
        "-DQINDAQT_INSTALL_COMPONENT=ControlsQmlModules"
        "-DQINDAQT_CONFIGURATION=$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_installed_controls_consumer.cmake"
)
set_tests_properties(
    qindaqt.controls-installed-import
    PROPERTIES LABELS "controls;package" RUN_SERIAL TRUE TIMEOUT 60
)

# AGENT-CONTRACT: qindaqt.controls-installed-import must pass after the
# lane-prescribed focused Controls build (the visual, behavior, font, and
# memory test targets), whose closure is exactly the two QML modules staged
# below. The product install rules for those modules live in the
# SettingsAppearanceRuntime component, whose closure also carries the Settings
# Center executable and every Settings route, so installing that component
# would require building unrelated libraries. This per-consumer component is
# declared from tests/controls because product paths under src/ are not this
# lane's to edit; it mirrors the TextEditor per-consumer component precedent
# in src/controls/CMakeLists.txt and must keep staging exactly the
# QindaQt/Controls and QindaQt/Tokens payloads, at the destinations and
# deploy paths the product rules use, with no additional files.
qt_query_qml_module(
    qindaqt_controls_qml
    QML_FILES qindaqt_controls_test_qml_files
    QML_FILES_DEPLOY_PATHS qindaqt_controls_test_qml_deploy_paths
)
foreach(qml_file deploy_path IN ZIP_LISTS
        qindaqt_controls_test_qml_files qindaqt_controls_test_qml_deploy_paths)
    if(NOT IS_ABSOLUTE "${qml_file}")
        set(qml_file "${PROJECT_SOURCE_DIR}/src/controls/${qml_file}")
    endif()
    cmake_path(GET deploy_path PARENT_PATH deploy_directory)
    cmake_path(GET deploy_path FILENAME deploy_name)
    install(
        FILES "${qml_file}"
        DESTINATION "${QT6_INSTALL_QML}/QindaQt/Controls/${deploy_directory}"
        RENAME "${deploy_name}"
        COMPONENT ControlsQmlModules
    )
endforeach()
install(
    TARGETS qindaqt_controls_qml qindaqt_controls_qmlplugin
    RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/Controls"
        COMPONENT ControlsQmlModules
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/Controls"
        COMPONENT ControlsQmlModules
    ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/Controls"
        COMPONENT ControlsQmlModules
)
install(
    FILES
        "${CMAKE_BINARY_DIR}/qml/QindaQt/Controls/qmldir"
        "${CMAKE_BINARY_DIR}/qml/QindaQt/Controls/qindaqt_controls.qmltypes"
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/Controls"
    COMPONENT ControlsQmlModules
)
install(
    TARGETS qindaqt_tokens_qml qindaqt_tokens_qmlplugin
    RUNTIME DESTINATION "${QT6_INSTALL_QML}/QindaQt/Tokens"
        COMPONENT ControlsQmlModules
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/Tokens"
        COMPONENT ControlsQmlModules
    ARCHIVE DESTINATION "${QT6_INSTALL_QML}/QindaQt/Tokens"
        COMPONENT ControlsQmlModules
)
install(
    FILES
        "${CMAKE_BINARY_DIR}/qml/QindaQt/Tokens/qmldir"
        "${CMAKE_BINARY_DIR}/qml/QindaQt/Tokens/qindaqt_tokens.qmltypes"
    DESTINATION "${QT6_INSTALL_QML}/QindaQt/Tokens"
    COMPONENT ControlsQmlModules
)
