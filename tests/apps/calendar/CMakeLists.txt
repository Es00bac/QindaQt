# SPDX-License-Identifier: GPL-3.0-or-later

function(qindaqt_add_calendar_test target source test_name)
    qt_add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE qindaqt_calendar_support Qt6::Test)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_features(${target} PRIVATE cxx_std_20)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    qindaqt_enable_warnings(${target})
    add_test(NAME ${test_name} COMMAND ${target})
    set_tests_properties(${test_name} PROPERTIES LABELS "calendar")
endfunction()

qindaqt_add_calendar_test(
    qindaqt_calendar_event_store_tests
    tst_event_store.cpp
    qindaqt.calendar-event-store
)
qindaqt_add_calendar_test(
    qindaqt_calendar_collection_tests
    tst_calendar_collection.cpp
    qindaqt.calendar-collection
)
qindaqt_add_calendar_test(
    qindaqt_calendar_occurrence_expander_tests
    tst_occurrence_expander.cpp
    qindaqt.calendar-occurrence-expander
)
qindaqt_add_calendar_test(
    qindaqt_calendar_reminder_scheduler_tests
    tst_reminder_scheduler.cpp
    qindaqt.calendar-reminder-scheduler
)
qindaqt_add_calendar_test(
    qindaqt_calendar_reminder_delivery_tests
    tst_reminder_delivery.cpp
    qindaqt.calendar-reminder-delivery
)
qindaqt_add_calendar_test(
    qindaqt_calendar_action_catalog_tests
    tst_calendar_action_catalog.cpp
    qindaqt.calendar-action-catalog
)
qindaqt_add_calendar_test(
    qindaqt_calendar_event_update_tests
    tst_event_update.cpp
    qindaqt.calendar-event-update
)
qindaqt_add_calendar_test(
    qindaqt_calendar_ics_import_export_tests
    tst_ics_import_export.cpp
    qindaqt.calendar-ics-import-export
)
target_compile_definitions(
    qindaqt_calendar_ics_import_export_tests
    PRIVATE QINDAQT_CALENDAR_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)

add_test(
    NAME qindaqt.calendar-desktop-metadata
    COMMAND "${CMAKE_COMMAND}"
            "-DDESKTOP_FILE=${PROJECT_SOURCE_DIR}/src/apps/calendar/org.qindaqt.Calendar.desktop"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_desktop_metadata.cmake"
)
set_tests_properties(qindaqt.calendar-desktop-metadata PROPERTIES LABELS "calendar")

add_test(
    NAME qindaqt.calendar-cli-rejects-multiple-paths
    COMMAND "${CMAKE_COMMAND}"
            "-DCALENDAR_EXECUTABLE=$<TARGET_FILE:qindaqt-calendar>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_cli_rejection.cmake"
)
set_tests_properties(
    qindaqt.calendar-cli-rejects-multiple-paths
    PROPERTIES LABELS "calendar" ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# Offscreen UI gates mirror the file-manager rows: they drive the production
# QML against a disposable data root with no session bus and fatal warnings.
add_test(
    NAME qindaqt.calendar-ui-contract-offscreen
    COMMAND $<TARGET_FILE:qindaqt-calendar> --check-ui-contract
)
set_tests_properties(
    qindaqt.calendar-ui-contract-offscreen
    PROPERTIES
        LABELS "calendar"
        ENVIRONMENT
            "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1;QT_QPA_PLATFORMTHEME=generic;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/ui-data"
        ENVIRONMENT_MODIFICATION
            "DISPLAY=unset:;WAYLAND_DISPLAY=unset:;DBUS_SESSION_BUS_ADDRESS=unset:"
)

add_test(
    NAME qindaqt.calendar-ui-actions-offscreen
    COMMAND $<TARGET_FILE:qindaqt-calendar>
            --check-ui-actions "${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)
set_tests_properties(
    qindaqt.calendar-ui-actions-offscreen
    PROPERTIES
        LABELS "calendar"
        ENVIRONMENT
            "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1;QT_QPA_PLATFORMTHEME=generic;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/ui-action-data"
        ENVIRONMENT_MODIFICATION
            "DISPLAY=unset:;WAYLAND_DISPLAY=unset:;DBUS_SESSION_BUS_ADDRESS=unset:"
)

# AGENT-CONTRACT: This row stages only the Calendar component and clears
# ambient build/import/library lookup. Its probes must therefore load the
# executable and the real QML root solely from the disposable relocatable
# prefix. ADR-0116: the component carries no importable Tokens/Controls QML
# modules or theme catalog; the row asserts their absence and requires the
# AppShell/Controls/Tokens backing shared libraries the executable's dynamic
# dependency chain still needs. It exists because the pre-fix package shipped
# a calendar whose RUNPATH could not resolve libqindaqt_app_shell.so.
add_test(
    NAME qindaqt.calendar-installed-runtime
    COMMAND
        "${CMAKE_COMMAND}"
        "-DQINDAQT_CMAKE=${CMAKE_COMMAND}"
        "-DQINDAQT_BUILD_DIRECTORY=${CMAKE_BINARY_DIR}"
        "-DQINDAQT_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/installed-calendar-stage"
        "-DQINDAQT_INSTALL_BINDIR=${CMAKE_INSTALL_BINDIR}"
        "-DQINDAQT_INSTALL_DATADIR=${CMAKE_INSTALL_DATADIR}"
        "-DQINDAQT_QML_INSTALL_DIR=${QT6_INSTALL_QML}"
        "-DQINDAQT_CONFIGURATION=$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_installed_calendar.cmake"
)
set_tests_properties(
    qindaqt.calendar-installed-runtime
    PROPERTIES LABELS "calendar;package" RUN_SERIAL TRUE TIMEOUT 180
)
