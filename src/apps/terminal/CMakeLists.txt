# SPDX-License-Identifier: GPL-3.0-or-later

# ADR-0028: qtermwidget6 is mandatory for the terminal target and its headers
# must not leak to any other module. The app support library below stays
# free of qtermwidget so policy/session/presentation tests link Qt only.
find_package(qtermwidget6 REQUIRED)

set(QINDAQT_TERMINAL_SUPPORT_SOURCES
    session/process_liveness.cpp
    session/terminal_launch_policy.cpp
    session/terminal_session.cpp
    session/terminal_session_backend.cpp
    ui/terminal_appearance.cpp
    ui/terminal_window.cpp
)
set(QINDAQT_TERMINAL_SUPPORT_HEADERS
    session/process_liveness.h
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

# The adapter is the only place qtermwidget6 is linked (ADR-0028). Keeping it
# out of the support library makes the boundary enforceable at link time.
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
    PUBLIC qindaqt_terminal_support qtermwidget6
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
