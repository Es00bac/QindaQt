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
