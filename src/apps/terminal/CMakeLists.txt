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
    session/process_liveness.cpp
    session/pty_bridge.cpp
    session/terminal_launch_policy.cpp
    session/terminal_session.cpp
    session/terminal_session_backend.cpp
    ui/terminal_appearance.cpp
    ui/terminal_window.cpp
)
set(QINDAQT_TERMINAL_SUPPORT_HEADERS
    session/process_liveness.h
    session/pty_bridge.h
    session/terminal_launch_policy.h
    session/terminal_session.h
    session/terminal_session_backend.h
    session/terminal_session_types.h
    ui/terminal_appearance.h
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
target_link_libraries(
    qindaqt_terminal_support
    PUBLIC QindaQt::DesignTokens qindaqt_themes Qt6::Core Qt6::Gui
           Qt6::Widgets
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
    qindaqt-terminal PRIVATE qindaqt_terminal_adapter
)
target_compile_features(qindaqt-terminal PRIVATE cxx_std_20)
set_target_properties(qindaqt-terminal PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt-terminal)

install(
    TARGETS qindaqt-terminal
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
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
