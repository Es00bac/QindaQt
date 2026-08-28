# SPDX-License-Identifier: LGPL-3.0-or-later

# Launcher L0 is wired through the repository root and may also be configured
# through the focused standalone test route.
qt_add_library(qindaqt_shell_launcher STATIC)
add_library(QindaQt::ShellLauncher ALIAS qindaqt_shell_launcher)

target_sources(
    qindaqt_shell_launcher
    PRIVATE
        src/application_catalog.cpp
        src/desktop_entry_parser.cpp
        src/launcher_category_model.cpp
        src/launcher_pinned_recent.cpp
        src/launcher_presentation.cpp
        src/launcher_search_ranker.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell_launcher/application_catalog.h
            include/qindaqt/shell_launcher/desktop_entry_parser.h
            include/qindaqt/shell_launcher/launcher_bounds.h
            include/qindaqt/shell_launcher/launcher_category_model.h
            include/qindaqt/shell_launcher/launcher_pinned_recent.h
            include/qindaqt/shell_launcher/launcher_presentation.h
            include/qindaqt/shell_launcher/launcher_search_ranker.h
            include/qindaqt/shell_launcher/launcher_types.h
)

# AGENT-CONTRACT: Qt Core only. The launcher model must never link Qt Gui,
# Qt Quick, Qt DBus, or KDE frameworks; platform adaptation (entry scanning,
# process execution, persistence) lives behind later boundaries owned by
# other modules (see docs/wiki/adr/0042-launcher-model-without-execution.md).
target_link_libraries(qindaqt_shell_launcher PUBLIC Qt6::Core)
target_compile_features(qindaqt_shell_launcher PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_launcher PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_launcher)

install(
    TARGETS qindaqt_shell_launcher
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
