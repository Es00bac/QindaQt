# SPDX-License-Identifier: GPL-3.0-or-later

# ADR-0040 (Accepted, superseding ADR-0030): qtermwidget6 is mandatory for
# the terminal target and its headers must not leak to any other module. The
# adapter + executable also never reach the build without the library. The
# audited integration contract is pinned to the 2.4.x series: upstream
# generates AnyNewerVersion config version files, so the exclusive-range
# request fails configuration on an unaudited 2.5+ instead of silently
# admitting it. No support-library test links the library (see
# tests/apps/terminal).
find_package(qtermwidget6 2.4...<2.5 REQUIRED)

set(QINDAQT_TERMINAL_SUPPORT_SOURCES
    app_shell/terminal_action_catalog.cpp
    app_shell/terminal_app_shell_bridge.cpp
    links/terminal_link.cpp
    links/terminal_link_opener.cpp
    profiles/terminal_profile.cpp
    profiles/terminal_profile_settings.cpp
    session/process_liveness.cpp
    session/pty_bridge.cpp
    session/terminal_launch_policy.cpp
    session/terminal_session.cpp
    session/terminal_session_backend.cpp
    session/terminal_session_collection.cpp
    search/terminal_search.cpp
    ui/terminal_appearance.cpp
    ui/terminal_profile_apply_status.cpp
    ui/terminal_profile_dialog.cpp
    ui/terminal_find_bar.cpp
    ui/terminal_window.cpp
    ui/terminal_window_actions.cpp
    ui/terminal_window_links.cpp
    ui/terminal_window_search.cpp
    ui/terminal_window_status.cpp
)
set(QINDAQT_TERMINAL_SUPPORT_HEADERS
    app_shell/terminal_action_catalog.h
    app_shell/terminal_app_shell_bridge.h
    links/terminal_link.h
    links/terminal_link_opener.h
    profiles/terminal_profile.h
    profiles/terminal_profile_settings.h
    session/process_liveness.h
    session/pty_bridge.h
    session/terminal_launch_policy.h
    session/terminal_session.h
    session/terminal_session_backend.h
    session/terminal_session_collection.h
    session/terminal_session_types.h
    search/terminal_search.h
    ui/terminal_appearance.h
    ui/terminal_profile_apply_status.h
    ui/terminal_profile_dialog.h
    ui/terminal_find_bar.h
    ui/terminal_window.h
)

qt_add_library(qindaqt_terminal_support STATIC)
target_sources(
    qindaqt_terminal_support
    PRIVATE ${QINDAQT_TERMINAL_SUPPORT_SOURCES}
    PUBLIC ${QINDAQT_TERMINAL_SUPPORT_HEADERS}
)
target_include_directories(
    qindaqt_terminal_support PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
)
# AppShell is consumed exactly like the Text Editor (ADR-0027): the C++
# coordinator and action catalog are public, the QML surface stays unused.
# The Settings1 client is a private implementation dependency of the
# profile persistence controller (ADR-0028); its headers stay out of the
# support library's public include surface.
target_link_libraries(
    qindaqt_terminal_support
    PUBLIC QindaQt::AppShell QindaQt::DesignTokens qindaqt_themes Qt6::Core
           Qt6::Gui Qt6::Widgets
)
target_link_libraries(
    qindaqt_terminal_support
    PRIVATE qindaqt_settings_client
)
target_compile_features(qindaqt_terminal_support PUBLIC cxx_std_20)
set_target_properties(qindaqt_terminal_support PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_terminal_support)

# The adapter is the only place qtermwidget6 is linked (ADR-0040). The link
# is PRIVATE (P2-3): consumers of the static adapter gain no qtermwidget
# include paths or usage requirements, keeping the dependency invisible to
# every other module even at CMake usage-requirement level.
qt_add_library(qindaqt_terminal_adapter STATIC)
target_sources(
    qindaqt_terminal_adapter
    PRIVATE ui/terminal_widget_adapter.cpp
            ui/terminal_widget_adapter_appearance.cpp
            ui/terminal_widget_adapter_search.cpp
            ui/terminal_widget_adapter_transport.cpp
    PUBLIC ui/terminal_widget_adapter.h
)
target_include_directories(
    qindaqt_terminal_adapter PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(
    qindaqt_terminal_adapter
    PUBLIC qindaqt_terminal_support
)
target_link_libraries(
    qindaqt_terminal_adapter
    PRIVATE qtermwidget6
)
target_compile_features(qindaqt_terminal_adapter PUBLIC cxx_std_20)
set_target_properties(qindaqt_terminal_adapter PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_terminal_adapter)

qt_add_executable(qindaqt-terminal main.cpp)
target_link_libraries(
    qindaqt-terminal PRIVATE qindaqt_terminal_adapter qindaqt_settings_client
                             QindaQt::AppAppearance
                             QindaQt::AppShellMenuExport QindaQt::FontDiscovery
)
target_compile_features(qindaqt-terminal PRIVATE cxx_std_20)

# AGENT-CONTRACT: qindaqt_app_shell is QindaQt::AppShell's SHARED backing
# library (src/app_shell/CMakeLists.txt), installed beside the QML module
# tree rather than CMAKE_INSTALL_BINDIR. Without this relative RPATH the
# installed terminal cannot resolve libqindaqt_app_shell.so even though the
# build tree links successfully. Mirrors src/apps/text_editor.
set(qindaqt_terminal_install_bindir "${CMAKE_INSTALL_BINDIR}")
cmake_path(
    ABSOLUTE_PATH qindaqt_terminal_install_bindir
    BASE_DIRECTORY "${CMAKE_INSTALL_PREFIX}"
    NORMALIZE
    OUTPUT_VARIABLE qindaqt_terminal_install_bindir_absolute
)
set(qindaqt_terminal_install_qmldir "${QT6_INSTALL_QML}")
cmake_path(
    ABSOLUTE_PATH qindaqt_terminal_install_qmldir
    BASE_DIRECTORY "${CMAKE_INSTALL_PREFIX}"
    NORMALIZE
    OUTPUT_VARIABLE qindaqt_terminal_install_qmldir_absolute
)
cmake_path(
    RELATIVE_PATH qindaqt_terminal_install_qmldir_absolute
    BASE_DIRECTORY "${qindaqt_terminal_install_bindir_absolute}"
    OUTPUT_VARIABLE qindaqt_terminal_qml_relative_path
)
set_target_properties(
    qindaqt-terminal
    PROPERTIES
        CXX_EXTENSIONS OFF
        INSTALL_RPATH "$ORIGIN/${qindaqt_terminal_qml_relative_path}/QindaQt/AppShell"
)
qindaqt_enable_warnings(qindaqt-terminal)

install(
    TARGETS qindaqt-terminal
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT Terminal
)
# The Terminal component is self-contained: its C++ AppShell action catalog
# dependency and AppShell's linked Controls/Tokens backing libraries are
# installed in their established sibling module directories. Declaring the
# component mappings here keeps this application-owned packaging rule out of
# the shared modules.
install(
    TARGETS qindaqt_app_shell
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/AppShell"
    COMPONENT Terminal
)
install(
    TARGETS qindaqt_controls_qml
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/Controls"
    COMPONENT Terminal
)
install(
    TARGETS qindaqt_tokens_qml
    LIBRARY DESTINATION "${QT6_INSTALL_QML}/QindaQt/Tokens"
    COMPONENT Terminal
)
install(
    FILES org.qindaqt.Terminal.desktop
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
    COMPONENT Terminal
)
install(
    DIRECTORY "${PROJECT_SOURCE_DIR}/data/themes/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/themes"
    COMPONENT Terminal
)
