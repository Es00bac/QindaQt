# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Test QuickTest)

# 1. Model unit tests
qt_add_executable(qindaqt_clipboard_applet_model_tests tst_clipboard_applet_model.cpp)
target_link_libraries(
    qindaqt_clipboard_applet_model_tests
    PRIVATE
        QindaQt::ShellClipboardApplet
        QindaQt::ClipboardModel
        Qt6::Core
        Qt6::Test
)
qindaqt_enable_warnings(qindaqt_clipboard_applet_model_tests)
add_test(
    NAME qindaqt.clipboard-applet-model
    COMMAND qindaqt_clipboard_applet_model_tests
)

# 2. Controller unit tests
qt_add_executable(qindaqt_clipboard_applet_controller_tests tst_clipboard_applet_controller.cpp)
target_link_libraries(
    qindaqt_clipboard_applet_controller_tests
    PRIVATE
        QindaQt::ShellClipboardApplet
        QindaQt::ClipboardModel
        Qt6::Core
        Qt6::Test
)
qindaqt_enable_warnings(qindaqt_clipboard_applet_controller_tests)
add_test(
    NAME qindaqt.clipboard-applet-controller
    COMMAND qindaqt_clipboard_applet_controller_tests
)

# 3. Client Seam tests
qt_add_executable(qindaqt_clipboard_applet_seam_tests tst_clipboard_applet_seam.cpp)
target_link_libraries(
    qindaqt_clipboard_applet_seam_tests
    PRIVATE
        QindaQt::ShellClipboardApplet
        QindaQt::ClipboardModel
        Qt6::Core
        Qt6::Test
)
qindaqt_enable_warnings(qindaqt_clipboard_applet_seam_tests)
add_test(
    NAME qindaqt.clipboard-applet-seam
    COMMAND qindaqt_clipboard_applet_seam_tests
)

# AGENT-GUARD: every QML row runs through run_clipboard_applet_qml_test.cmake,
# which first builds this prerequisite target. A bare qmltestrunner add_test()
# has no executable prerequisite, so a focused build of the declared C++ test
# targets leaves the applet/Controls/Tokens QML plugins and the two applet
# manifest binaries unbuilt and all QML rows fail at import time.
add_custom_target(
    qindaqt_clipboard_applet_qml_test_prerequisites
    DEPENDS
        qindaqt_shell_clipboard_appletplugin
        qindaqt_controls_qmlplugin
        qindaqt_tokens_qmlplugin
        qindaqt_applet_manifest_tests
        qindaqt_applet_catalog_tests
)

function(qindaqt_add_clipboard_applet_qml_test test_name test_input)
    add_test(
        NAME ${test_name}
        COMMAND
            "${CMAKE_COMMAND}"
            "-DQINDAQT_CMAKE=${CMAKE_COMMAND}"
            "-DQINDAQT_BUILD_DIRECTORY=${CMAKE_BINARY_DIR}"
            "-DQINDAQT_CONFIGURATION=$<CONFIG>"
            "-DQINDAQT_PREREQUISITE_TARGET=qindaqt_clipboard_applet_qml_test_prerequisites"
            "-DQINDAQT_QMLTESTRUNNER=$<TARGET_FILE:Qt6::qmltestrunner>"
            "-DQINDAQT_QML_IMPORT_PATH=${CMAKE_BINARY_DIR}/qml"
            "-DQINDAQT_TEST_INPUT=${test_input}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_clipboard_applet_qml_test.cmake"
    )
    set_tests_properties(
        ${test_name}
        PROPERTIES LABELS "offscreen" TIMEOUT 300
    )
endfunction()

# 4. QML Offscreen tests (build prerequisites made explicit by the driver)
qindaqt_add_clipboard_applet_qml_test(
    qindaqt.clipboard-applet-qml-offscreen
    "${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_ClipboardApplet.qml"
)
qindaqt_add_clipboard_applet_qml_test(
    qindaqt.clipboard-applet-qml-accessibility-offscreen
    "${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_ClipboardAppletAccessibility.qml"
)
qindaqt_add_clipboard_applet_qml_test(
    qindaqt.clipboard-applet-qml-keyboard-offscreen
    "${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_ClipboardAppletKeyboard.qml"
)

# 5. Boundary & Source Policy test
add_test(
    NAME qindaqt.clipboard-applet-boundary-policy
    COMMAND
        "${CMAKE_COMMAND}"
        "-DQINDAQT_CLIPBOARD_APPLET_SOURCE_DIR=${CMAKE_SOURCE_DIR}/src/shell/clipboard_applet"
        "-DQINDAQT_CLIPBOARD_APPLET_POISON_DIRECTORY=${CMAKE_BINARY_DIR}/tests/clipboard_applet_poison_probe"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_clipboard_applet_boundary.cmake"
)

# 6. Installed component consumer/probe: validates the ClipboardApplet install
# rules end-to-end (public boundary, library, QML module, plugin, manifest)
# from a fresh staged prefix.
add_test(
    NAME qindaqt.clipboard-applet-installed-consumer
    COMMAND
        "${CMAKE_COMMAND}"
        "-DQINDAQT_CMAKE=${CMAKE_COMMAND}"
        "-DQINDAQT_BUILD_DIRECTORY=${CMAKE_BINARY_DIR}"
        "-DQINDAQT_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/installed-clipboard-applet-stage"
        "-DQINDAQT_INSTALL_INCLUDEDIR=${CMAKE_INSTALL_INCLUDEDIR}"
        "-DQINDAQT_QML_INSTALL_DIR=${QT6_INSTALL_QML}"
        "-DQINDAQT_INSTALL_DATADIR=${CMAKE_INSTALL_DATADIR}"
        "-DQINDAQT_CONFIGURATION=$<CONFIG>"
        "-DQINDAQT_CONTROLS_MODULE_DIRECTORY=${CMAKE_BINARY_DIR}/qml/QindaQt/Controls"
        "-DQINDAQT_TOKENS_MODULE_DIRECTORY=${CMAKE_BINARY_DIR}/qml/QindaQt/Tokens"
        "-DQINDAQT_CONTROLS_LIBRARY=$<TARGET_FILE:qindaqt_controls_qml>"
        "-DQINDAQT_TOKENS_LIBRARY=$<TARGET_FILE:qindaqt_tokens_qml>"
        "-DQINDAQT_CONSUMER_SOURCE_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer"
        "-DQINDAQT_CONSUMER_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/installed_cpp_consumer.cpp"
        "-DQINDAQT_QMLTESTRUNNER=$<TARGET_FILE:Qt6::qmltestrunner>"
        "-DQINDAQT_QML_TEST=${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_InstalledClipboardApplet.qml"
        "-DQINDAQT_QT6_DIR=${Qt6_DIR}"
        "-DQINDAQT_GENERATOR=${CMAKE_GENERATOR}"
        "-DQINDAQT_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DQINDAQT_SHARED_LIBRARY_SUFFIX=${CMAKE_SHARED_LIBRARY_SUFFIX}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_installed_clipboard_applet.cmake"
)
set_tests_properties(
    qindaqt.clipboard-applet-installed-consumer
    PROPERTIES LABELS "package" RUN_SERIAL TRUE TIMEOUT 300
)
