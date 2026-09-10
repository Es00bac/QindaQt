# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_library(qindaqt_calendar_support STATIC)
target_sources(
    qindaqt_calendar_support
    PRIVATE
        app_shell/calendar_action_catalog.cpp
        ics/ics_import_export.cpp
        model/calendar_collection.cpp
        model/calendar_controller.cpp
        model/event_store.cpp
        model/occurrence_expander.cpp
        model/occurrence_list_model.cpp
        model/reminder_delivery.cpp
        model/reminder_scheduler.cpp
        settings/calendar_preferences.cpp
    PUBLIC
        app_shell/calendar_action_catalog.h
        ics/ics_import_export.h
        model/calendar_collection.h
        model/calendar_controller.h
        model/calendar_types.h
        model/event_store.h
        model/occurrence_expander.h
        model/occurrence_list_model.h
        model/reminder_delivery.h
        model/reminder_scheduler.h
        settings/calendar_preferences.h
)
target_include_directories(qindaqt_calendar_support PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(
    qindaqt_calendar_support
    PUBLIC
        QindaQt::AppShell
        QindaQt::AppShellMenuExport
        QindaQt::SettingsClient
        KF6::CalendarCore
        Qt6::Core
        Qt6::DBus
        Qt6::Gui
)
target_compile_features(qindaqt_calendar_support PUBLIC cxx_std_20)
set_target_properties(qindaqt_calendar_support PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_calendar_support)

qt_add_executable(qindaqt-calendar
    main.cpp
    runtime/ui_action_probe.cpp
    runtime/ui_action_probe.h
)
# ui_action_probe.h matches AUTOUIC's ui_*.h pattern; this target has no
# Designer forms.
set_target_properties(qindaqt-calendar PROPERTIES AUTOUIC OFF)
qt_add_qml_module(
    qindaqt-calendar
    URI QindaQt.CalendarApp
    VERSION 1.0
    QML_FILES
        ui/Main.qml
        ui/MonthViewGrid.qml
        ui/WeekView.qml
        ui/DayView.qml
        ui/CalendarSidebar.qml
        ui/EventDetailsPane.qml
        ui/EventEditorDialog.qml
        ui/ImportExportDialog.qml
)

# ADR-0116: Calendar renders with stock Qt Quick Controls; it links only the
# non-visual seams (AppShell coordinator/catalog, menu export, Settings1,
# F1 font bootstrap). No QindaQt.Tokens/Controls QML modules, no theme
# catalog, no per-app palette.
set(qindaqt_calendar_install_bindir "${CMAKE_INSTALL_BINDIR}")
cmake_path(
    ABSOLUTE_PATH qindaqt_calendar_install_bindir
    BASE_DIRECTORY "${CMAKE_INSTALL_PREFIX}"
    NORMALIZE
    OUTPUT_VARIABLE qindaqt_calendar_install_bindir_absolute
)
set(qindaqt_calendar_install_qmldir "${QT6_INSTALL_QML}")
cmake_path(
    ABSOLUTE_PATH qindaqt_calendar_install_qmldir
    BASE_DIRECTORY "${CMAKE_INSTALL_PREFIX}"
    NORMALIZE
    OUTPUT_VARIABLE qindaqt_calendar_install_qmldir_absolute
)
cmake_path(
    RELATIVE_PATH qindaqt_calendar_install_qmldir_absolute
    BASE_DIRECTORY "${qindaqt_calendar_install_bindir_absolute}"
    OUTPUT_VARIABLE qindaqt_calendar_qml_relative_path
)

# AGENT-CONTRACT: The executable dynamically links libqindaqt_app_shell.so
# (the non-visual coordinator seam), whose backing library in turn needs the
# Controls/Tokens backing libraries. The staged Calendar component resolves
# that chain solely from its own payload: the executable's RPATH reaches the
# AppShell backing library, and the backing libraries' own INSTALL_RPATHs
# ($ORIGIN/../Tokens, $ORIGIN/../Controls) reach their siblings. Keep the
# install(TARGETS) rules below and this RPATH in sync.
target_link_libraries(
    qindaqt-calendar
    PRIVATE
        qindaqt_calendar_support
        QindaQt::AppShell
        QindaQt::FontDiscovery
        Qt6::Core
        Qt6::DBus
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)
target_compile_features(qindaqt-calendar PRIVATE cxx_std_20)
set_target_properties(
    qindaqt-calendar
    PROPERTIES
        CXX_EXTENSIONS OFF
        INSTALL_RPATH
            "$ORIGIN/${qindaqt_calendar_qml_relative_path}/QindaQt/AppShell"
)
qindaqt_enable_warnings(qindaqt-calendar)

install(
    TARGETS qindaqt-calendar
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT Calendar
)
install(
    FILES org.qindaqt.Calendar.desktop
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
    COMPONENT Calendar
)

# ADR-0116: the Calendar component ships no importable QindaQt QML modules
# (no qmldir, typeinfo, or QML sources) and no theme catalog. It still repeats
# the three backing shared libraries named by the executable's dynamic
# dependency chain so a component-only staged install resolves
# libqindaqt_app_shell.so and its Controls/Tokens siblings without the rest of
# the shell install. The chain relies on each library's own INSTALL_RPATH; see
# the AGENT-CONTRACT above.
install(
    TARGETS qindaqt_app_shell
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/AppShell"
    COMPONENT Calendar
)
install(
    TARGETS qindaqt_controls_qml
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/Controls"
    COMPONENT Calendar
)
install(
    TARGETS qindaqt_tokens_qml
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/Tokens"
    COMPONENT Calendar
)
