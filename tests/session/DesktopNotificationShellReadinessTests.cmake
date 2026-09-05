# SPDX-License-Identifier: GPL-3.0-or-later

# Keep the one-shot ShellDevelopment evidence unit out of the already broad
# desktop runtime/package registry. The production probe consumes the same
# helper target sources from DesktopSessionTests.cmake.
qt_add_executable(
    qindaqt-desktop-notification-shell-readiness-tests
    "${CMAKE_CURRENT_SOURCE_DIR}/desktopnotificationshellreadiness.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/desktopnotificationshellreadiness.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/desktopnotificationshellpolish.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/desktopnotificationshellpolish.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/desktopnotificationshellpolishfixtures.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/desktopnotificationshellpolishfixtures.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/desktopnotificationshellsample.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/tst_desktopnotificationshellreadiness.cpp"
)
target_link_libraries(
    qindaqt-desktop-notification-shell-readiness-tests
    PRIVATE Qt6::Core Qt6::DBus Qt6::Gui Qt6::Test
)
set_target_properties(
    qindaqt-desktop-notification-shell-readiness-tests
    PROPERTIES CXX_EXTENSIONS OFF
)
qindaqt_enable_warnings(qindaqt-desktop-notification-shell-readiness-tests)
add_test(
    NAME desktop.virtual.notification-shell-readiness-unit
    COMMAND qindaqt-desktop-notification-shell-readiness-tests
)
set_tests_properties(
    desktop.virtual.notification-shell-readiness-unit
    PROPERTIES LABELS "unit;session;security;display;input"
)
