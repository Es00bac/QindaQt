# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus Gui Test Widgets)
find_program(QINDAQT_PORTAL_DBUS_DAEMON_EXECUTABLE NAMES dbus-daemon REQUIRED)
find_program(QINDAQT_PORTAL_DBUS_RUN_SESSION_EXECUTABLE
             NAMES dbus-run-session REQUIRED)
find_program(QINDAQT_XDG_DESKTOP_PORTAL_EXECUTABLE
             NAMES xdg-desktop-portal
             PATHS /usr/libexec /usr/lib/xdg-desktop-portal)
find_file(QINDAQT_KDE_PORTAL_METADATA
          NAMES kde.portal
          PATHS /usr/share/xdg-desktop-portal/portals)

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

if(QINDAQT_XDG_DESKTOP_PORTAL_EXECUTABLE AND QINDAQT_KDE_PORTAL_METADATA)
    qt_add_executable(qindaqt_portal_toolkit_probe portal_toolkit_probe.cpp)
    target_link_libraries(qindaqt_portal_toolkit_probe
                          PRIVATE Qt6::Gui Qt6::Widgets)
    qindaqt_enable_warnings(qindaqt_portal_toolkit_probe)

    qt_add_executable(qindaqt_portal_frontend_integration_tests
                      portal_frontend_test_support.cpp
                      portal_frontend_test_support.h
                      tst_portal_frontend_integration.cpp)
    target_compile_definitions(
        qindaqt_portal_frontend_integration_tests
        PRIVATE
            QINDAQT_FALLBACK_PORTAL="${QINDAQT_KDE_PORTAL_METADATA}"
            QINDAQT_PORTAL_EXECUTABLE="$<TARGET_FILE:xdg-desktop-portal-qindaqt>"
            QINDAQT_PORTAL_METADATA="${PROJECT_SOURCE_DIR}/src/services/portal/data/qindaqt.portal"
            QINDAQT_PORTAL_SELECTION="${PROJECT_SOURCE_DIR}/src/services/portal/data/qindaqt-portals.conf"
            QINDAQT_PORTAL_TEST_RUNTIME_ROOT="${CMAKE_CURRENT_BINARY_DIR}/portal-p1-runtime"
            QINDAQT_PORTAL_THEME_DIR="${PROJECT_SOURCE_DIR}/data/themes"
            QINDAQT_SETTINGS_EXECUTABLE="$<TARGET_FILE:qindaqt-settings-service>"
            QINDAQT_SETTINGS_SCHEMA_DIR="${PROJECT_SOURCE_DIR}/data/settings"
            QINDAQT_TOOLKIT_PROBE="$<TARGET_FILE:qindaqt_portal_toolkit_probe>"
            QINDAQT_XDG_DESKTOP_PORTAL="${QINDAQT_XDG_DESKTOP_PORTAL_EXECUTABLE}"
    )
    target_link_libraries(
        qindaqt_portal_frontend_integration_tests
        PRIVATE QindaQt::PortalAppearance QindaQt::SettingsProtocol
                Qt6::Core Qt6::DBus
    )
    add_dependencies(
        qindaqt_portal_frontend_integration_tests
        qindaqt-settings-service qindaqt_portal_toolkit_probe
        xdg-desktop-portal-qindaqt
    )
    qindaqt_enable_warnings(qindaqt_portal_frontend_integration_tests)

    foreach(mode IN ITEMS selection toolkit)
        add_test(
            NAME qindaqt.portal-frontend-${mode}
            COMMAND
                "${CMAKE_COMMAND}" -E env
                --unset=DBUS_SESSION_BUS_ADDRESS
                --unset=DBUS_STARTER_ADDRESS
                --unset=DBUS_STARTER_BUS_TYPE
                "${QINDAQT_PORTAL_DBUS_RUN_SESSION_EXECUTABLE}" --
                "$<TARGET_FILE:qindaqt_portal_frontend_integration_tests>"
                "${mode}"
        )
    endforeach()
    set(portal_p1_package_arguments
        "-DQINDAQT_FRONTEND_TEST=$<TARGET_FILE:qindaqt_portal_frontend_integration_tests>"
        "-DQINDAQT_TOOLKIT_PROBE=$<TARGET_FILE:qindaqt_portal_toolkit_probe>"
        "-DQINDAQT_XDG_DESKTOP_PORTAL=${QINDAQT_XDG_DESKTOP_PORTAL_EXECUTABLE}"
        "-DQINDAQT_FALLBACK_PORTAL=${QINDAQT_KDE_PORTAL_METADATA}"
        "-DQINDAQT_DBUS_RUN_SESSION=${QINDAQT_PORTAL_DBUS_RUN_SESSION_EXECUTABLE}"
    )
else()
    message(STATUS
        "xdg-desktop-portal or kde.portal not found; Portal P1 frontend rows are not registered")
    set(portal_p1_package_arguments)
endif()

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
        ${portal_p1_package_arguments}
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
if(TEST qindaqt.portal-frontend-selection)
    set_tests_properties(
        qindaqt.portal-frontend-selection qindaqt.portal-frontend-toolkit
        PROPERTIES LABELS "service;portal;frontend;dbus;hostile"
                   TIMEOUT 35 RUN_SERIAL TRUE
    )
endif()
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
if(TEST qindaqt.portal-frontend-selection)
    set_property(TEST qindaqt.portal-staged-package APPEND PROPERTY
                 DEPENDS "qindaqt.portal-frontend-selection;qindaqt.portal-frontend-toolkit")
endif()
