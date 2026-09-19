# SPDX-License-Identifier: GPL-3.0-or-later

set(viewer_test_root "${CMAKE_CURRENT_BINARY_DIR}/isolated")
file(MAKE_DIRECTORY "${viewer_test_root}/home" "${viewer_test_root}/runtime")
file(CHMOD "${viewer_test_root}/runtime" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
set(viewer_test_env
    "QT_QPA_PLATFORM=offscreen;QSG_RHI_BACKEND=software;QT_QUICK_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Basic;QT_QPA_PLATFORMTHEME=;HOME=${viewer_test_root}/home;XDG_CONFIG_HOME=${viewer_test_root}/home/config;XDG_DATA_HOME=${viewer_test_root}/home/data;XDG_RUNTIME_DIR=${viewer_test_root}/runtime;DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent;DISPLAY=;WAYLAND_DISPLAY=")
foreach(part IN ITEMS renderer controller)
    qt_add_executable(qindaqt_viewer_${part}_test tst_${part}.cpp)
    target_link_libraries(qindaqt_viewer_${part}_test PRIVATE qindaqt_viewer_support Qt6::Test)
    target_compile_definitions(qindaqt_viewer_${part}_test PRIVATE
        VIEWER_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
    qindaqt_enable_warnings(qindaqt_viewer_${part}_test)
    add_test(NAME apps.viewer.${part} COMMAND qindaqt_viewer_${part}_test)
    set_tests_properties(apps.viewer.${part} PROPERTIES ENVIRONMENT "${viewer_test_env}" TIMEOUT 45)
endforeach()

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/artifacts")
qt_add_executable(qindaqt_viewer_ui_test tst_ui.cpp
    "${PROJECT_SOURCE_DIR}/src/apps/viewer/viewer_actions.cpp")
target_link_libraries(qindaqt_viewer_ui_test PRIVATE qindaqt_viewer_support
    QindaQt::AppShell Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Test)
target_compile_definitions(qindaqt_viewer_ui_test PRIVATE
    VIEWER_MAIN_QML="${PROJECT_SOURCE_DIR}/src/apps/viewer/qml/Main.qml"
    VIEWER_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    VIEWER_ARTIFACTS_DIR="${CMAKE_CURRENT_BINARY_DIR}/artifacts")
qindaqt_enable_warnings(qindaqt_viewer_ui_test)
add_test(NAME apps.viewer.ui COMMAND qindaqt_viewer_ui_test)
set_tests_properties(apps.viewer.ui PROPERTIES ENVIRONMENT "${viewer_test_env}" TIMEOUT 45)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME apps.viewer.cli-install
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/verify_install.py"
        --binary "$<TARGET_FILE:qindaqt-viewer>"
        --build "${PROJECT_BINARY_DIR}"
        --root "${CMAKE_CURRENT_BINARY_DIR}/install-smoke")
set_tests_properties(apps.viewer.cli-install PROPERTIES ENVIRONMENT "${viewer_test_env}" TIMEOUT 60)
