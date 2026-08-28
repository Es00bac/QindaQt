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

# 4. QML Offscreen tests
add_test(
    NAME qindaqt.clipboard-applet-qml-offscreen
    COMMAND
        $<TARGET_FILE:Qt6::qmltestrunner>
        -import "${CMAKE_BINARY_DIR}/qml"
        -input "${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_ClipboardApplet.qml"
)
set_tests_properties(
    qindaqt.clipboard-applet-qml-offscreen
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QML2_IMPORT_PATH=${CMAKE_BINARY_DIR}/qml"
)

add_test(
    NAME qindaqt.clipboard-applet-qml-accessibility-offscreen
    COMMAND
        $<TARGET_FILE:Qt6::qmltestrunner>
        -import "${CMAKE_BINARY_DIR}/qml"
        -input "${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_ClipboardAppletAccessibility.qml"
)
set_tests_properties(
    qindaqt.clipboard-applet-qml-accessibility-offscreen
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QML2_IMPORT_PATH=${CMAKE_BINARY_DIR}/qml"
)

add_test(
    NAME qindaqt.clipboard-applet-qml-keyboard-offscreen
    COMMAND
        $<TARGET_FILE:Qt6::qmltestrunner>
        -import "${CMAKE_BINARY_DIR}/qml"
        -input "${CMAKE_CURRENT_SOURCE_DIR}/qml/tst_ClipboardAppletKeyboard.qml"
)
set_tests_properties(
    qindaqt.clipboard-applet-qml-keyboard-offscreen
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QML2_IMPORT_PATH=${CMAKE_BINARY_DIR}/qml"
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
