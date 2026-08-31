# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus Test)
find_program(QINDAQT_PORTAL_DBUS_DAEMON_EXECUTABLE NAMES dbus-daemon REQUIRED)

qt_add_executable(qindaqt_portal_appearance_policy_tests
                  tst_appearance_policy.cpp)
target_compile_definitions(
    qindaqt_portal_appearance_policy_tests
    PRIVATE QINDAQT_PORTAL_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
target_link_libraries(
    qindaqt_portal_appearance_policy_tests
    PRIVATE QindaQt::PortalAppearance Qt6::Test
)
qindaqt_enable_warnings(qindaqt_portal_appearance_policy_tests)
add_test(NAME qindaqt.portal-appearance-policy
         COMMAND qindaqt_portal_appearance_policy_tests)

qt_add_executable(qindaqt_portal_settings_source_tests
                  tst_settings1_appearance_source.cpp)
target_compile_definitions(
    qindaqt_portal_settings_source_tests
    PRIVATE QINDAQT_PORTAL_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
target_link_libraries(
    qindaqt_portal_settings_source_tests
    PRIVATE
        QindaQt::PortalService
        QindaQt::SettingsProtocol
        Qt6::DBus
        Qt6::Test
)
qindaqt_enable_warnings(qindaqt_portal_settings_source_tests)
add_test(NAME qindaqt.portal-settings1-source
         COMMAND qindaqt_portal_settings_source_tests)

qt_add_executable(qindaqt_portal_service_tests tst_portal_service.cpp)
target_compile_definitions(
    qindaqt_portal_service_tests
    PRIVATE
        QINDAQT_PORTAL_DBUS_DAEMON_EXECUTABLE="${QINDAQT_PORTAL_DBUS_DAEMON_EXECUTABLE}"
)
target_link_libraries(
    qindaqt_portal_service_tests
    PRIVATE QindaQt::PortalService Qt6::DBus Qt6::Test
)
qindaqt_enable_warnings(qindaqt_portal_service_tests)
add_test(NAME qindaqt.portal-service COMMAND qindaqt_portal_service_tests)

qt_add_executable(
    qindaqt_portal_process_lifecycle_tests
    tst_portal_process_lifecycle.cpp
)
target_compile_definitions(
    qindaqt_portal_process_lifecycle_tests
    PRIVATE
        QINDAQT_PORTAL_DBUS_DAEMON_EXECUTABLE="${QINDAQT_PORTAL_DBUS_DAEMON_EXECUTABLE}"
        QINDAQT_PORTAL_SERVICE_EXECUTABLE="$<TARGET_FILE:xdg-desktop-portal-qindaqt>"
        QINDAQT_SETTINGS_SERVICE_EXECUTABLE="$<TARGET_FILE:qindaqt-settings-service>"
        QINDAQT_PORTAL_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
target_link_libraries(
    qindaqt_portal_process_lifecycle_tests
    PRIVATE QindaQt::PortalService Qt6::Core Qt6::DBus Qt6::Test
)
add_dependencies(
    qindaqt_portal_process_lifecycle_tests
    qindaqt-settings-service xdg-desktop-portal-qindaqt
)
qindaqt_enable_warnings(qindaqt_portal_process_lifecycle_tests)
add_test(
    NAME qindaqt.portal-process-lifecycle
    COMMAND qindaqt_portal_process_lifecycle_tests
)

add_test(
    NAME qindaqt.portal-source-boundary
    COMMAND
        "${CMAKE_COMMAND}"
        "-DPORTAL_ROOT=${PROJECT_SOURCE_DIR}/src/services/portal"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake"
)
add_test(
    NAME qindaqt.portal-source-boundary-negative
    COMMAND
        "${CMAKE_COMMAND}"
        "-DSOURCE_PORTAL_ROOT=${PROJECT_SOURCE_DIR}/src/services/portal"
        "-DBINARY_ROOT=${CMAKE_BINARY_DIR}"
        "-DPOISON_ROOT=${CMAKE_CURRENT_BINARY_DIR}/portal-boundary-poison"
        "-DCHECK_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary_negative.cmake"
)
add_test(
    NAME qindaqt.portal-staged-package
    COMMAND
        "${CMAKE_COMMAND}"
        "-DQINDAQT_CMAKE=${CMAKE_COMMAND}"
        "-DQINDAQT_BUILD_DIRECTORY=${CMAKE_BINARY_DIR}"
        "-DQINDAQT_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/installed-portal-stage"
        "-DQINDAQT_INSTALL_INCLUDEDIR=${CMAKE_INSTALL_INCLUDEDIR}"
        "-DQINDAQT_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}"
        "-DQINDAQT_INSTALL_LIBEXECDIR=${KDE_INSTALL_LIBEXECDIR}"
        "-DQINDAQT_INSTALL_DBUSSERVICEDIR=${KDE_INSTALL_DBUSSERVICEDIR}"
        "-DQINDAQT_INSTALL_SYSTEMDUSERUNITDIR=${KDE_INSTALL_SYSTEMDUSERUNITDIR}"
        "-DQINDAQT_INSTALL_DATADIR=${CMAKE_INSTALL_DATADIR}"
        "-DQINDAQT_EXPECTED_PORTAL_EXECUTABLE=${KDE_INSTALL_FULL_LIBEXECDIR}/xdg-desktop-portal-qindaqt"
        "-DQINDAQT_EXPECTED_SETTINGS_EXECUTABLE=$<TARGET_FILE:qindaqt-settings-service>"
        "-DQINDAQT_EXPECTED_SCHEMA_DIR=${PROJECT_SOURCE_DIR}/data/settings"
        "-DQINDAQT_PROCESS_TEST=$<TARGET_FILE:qindaqt_portal_process_lifecycle_tests>"
        "-DCHECK_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake"
        "-DSOURCE_PORTAL_ROOT=${PROJECT_SOURCE_DIR}/src/services/portal"
        "-DQINDAQT_CONFIGURATION=$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_staged_package.cmake"
)

set_tests_properties(
    qindaqt.portal-appearance-policy qindaqt.portal-settings1-source
    PROPERTIES LABELS "service;portal;appearance" TIMEOUT 20
)
set_tests_properties(
    qindaqt.portal-service qindaqt.portal-process-lifecycle
    PROPERTIES LABELS "service;portal;appearance;dbus" TIMEOUT 20 RUN_SERIAL TRUE
)
set_tests_properties(
    qindaqt.portal-source-boundary qindaqt.portal-source-boundary-negative
    PROPERTIES LABELS "service;portal;appearance;boundary;hostile" TIMEOUT 20
)
set_tests_properties(
    qindaqt.portal-staged-package
    PROPERTIES
        LABELS "service;portal;appearance;package;dbus;hostile"
        TIMEOUT 40
        RUN_SERIAL TRUE
        DEPENDS "qindaqt.portal-process-lifecycle"
)
