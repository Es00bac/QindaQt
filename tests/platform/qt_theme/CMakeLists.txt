# SPDX-License-Identifier: GPL-3.0-or-later
find_package(Qt6 REQUIRED COMPONENTS GuiPrivate)
qt_add_executable(qindaqt_native_theme_tests tst_native_theme.cpp)
target_link_libraries(qindaqt_native_theme_tests PRIVATE qindaqt_native_palette QindaQt::SettingsProtocol Qt6::Widgets Qt6::Quick Qt6::Test)
target_compile_definitions(qindaqt_native_theme_tests PRIVATE QINDAQT_SOURCE_ROOT="${PROJECT_SOURCE_DIR}")
qindaqt_enable_warnings(qindaqt_native_theme_tests)
add_dependencies(qindaqt_native_theme_tests qindaqt_qt_platform_theme)
add_test(NAME qindaqt.qt-platform-theme COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/run_native_theme.py
    $<TARGET_FILE:qindaqt_native_theme_tests> ${CMAKE_BINARY_DIR}/plugins ${PROJECT_SOURCE_DIR})
set_tests_properties(qindaqt.qt-platform-theme PROPERTIES LABELS "qt-theme" TIMEOUT 45)

add_test(NAME qindaqt.qt-platform-theme-explicit-style COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/run_native_theme.py
    $<TARGET_FILE:qindaqt_native_theme_tests> ${CMAKE_BINARY_DIR}/plugins ${PROJECT_SOURCE_DIR} Windows)
set_tests_properties(qindaqt.qt-platform-theme-explicit-style PROPERTIES LABELS "qt-theme" TIMEOUT 45)
add_test(NAME qindaqt.qt-platform-theme-installed COMMAND ${CMAKE_COMMAND}
    -DBUILD_ROOT=${CMAKE_BINARY_DIR} -DINSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}
    -DPROBE=$<TARGET_FILE:qindaqt_native_theme_tests>
    -DSOURCE_ROOT=${PROJECT_SOURCE_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/run_installed_native_theme.cmake)
set_tests_properties(qindaqt.qt-platform-theme-installed PROPERTIES LABELS "qt-theme;package" RUN_SERIAL TRUE TIMEOUT 60)

qt_add_executable(qindaqt_platform_services_tests tst_platform_services.cpp
    ${PROJECT_SOURCE_DIR}/src/platform/qt_theme/qindaqt_platform_theme.cpp
    ${PROJECT_SOURCE_DIR}/src/platform/qt_theme/qindaqt_platform_theme.h)
target_link_libraries(qindaqt_platform_services_tests PRIVATE qindaqt_native_palette QindaQt::AppAppearance Qt6::GuiPrivate Qt6::Widgets Qt6::Test)
qindaqt_enable_warnings(qindaqt_platform_services_tests)
add_test(NAME qindaqt.qt-platform-theme-services COMMAND qindaqt_platform_services_tests)
set_tests_properties(qindaqt.qt-platform-theme-services PROPERTIES LABELS "qt-theme"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORMTHEME=generic;QT_FATAL_WARNINGS=1"
    ENVIRONMENT_MODIFICATION "DISPLAY=unset:;WAYLAND_DISPLAY=unset:;DBUS_SESSION_BUS_ADDRESS=unset:")
