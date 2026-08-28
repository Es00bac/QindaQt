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
        "-DQINDAQT_CONFIGURATION=$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_installed_controls_consumer.cmake"
)
set_tests_properties(
    qindaqt.controls-installed-import
    PROPERTIES LABELS "controls;package" RUN_SERIAL TRUE TIMEOUT 60
)
