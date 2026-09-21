# SPDX-License-Identifier: GPL-3.0-or-later

# The gather overview's presentation model. Pure: it maps the task-list applet
# projection onto the hybrid_gather planner's rectangles. No Qt GUI, no KWin,
# no global state - Qt6::Core only, so it tests without a display.
qt_add_library(qindaqt_shell_gather_overview STATIC)
add_library(QindaQt::ShellGatherOverview ALIAS qindaqt_shell_gather_overview)

target_sources(
    qindaqt_shell_gather_overview
    PRIVATE
        src/gather_overview_projection.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/gather_overview/gather_overview_projection.h
)

target_link_libraries(
    qindaqt_shell_gather_overview
    PUBLIC
        Qt6::Core
        QindaQt::HybridGather
        QindaQt::ShellTaskListApplet
)
target_compile_features(qindaqt_shell_gather_overview PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_shell_gather_overview
    PROPERTIES
        CXX_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE ON
)

if(COMMAND qindaqt_enable_warnings)
    qindaqt_enable_warnings(qindaqt_shell_gather_overview)
endif()
