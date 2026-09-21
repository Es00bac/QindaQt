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
        src/gather_overview_controller.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/gather_overview/gather_overview_projection.h
            include/qindaqt/shell/gather_overview/gather_overview_controller.h
)

# The controller is a QObject, so it needs moc. It still links Qt6::Core only:
# Q_PROPERTY, Q_INVOKABLE and QVariantMap are all Core, which keeps the whole
# model half qualifiable without a display.
set_target_properties(qindaqt_shell_gather_overview PROPERTIES AUTOMOC ON)

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

# The drawing surface, as its own QML module. Kept separate from the model
# above on purpose: the model must stay Qt6::Core so it can be qualified
# without a display, and a QML module drags in Qml/Quick/Gui.
#
# AGENT-CONTRACT: the surface consumes the projection value object plus
# QindaQt.Controls, QindaQt.Shell.Icons and QindaQt.Tokens. It must never link
# D-Bus, KWin, LayerShellQt, the shell runtime, or the window-actions client:
# it draws what it is handed and reports what was clicked, and every decision
# above that belongs to its owner.
find_package(Qt6 6.11 REQUIRED COMPONENTS Qml Quick Svg)

qt_add_library(qindaqt_shell_gather_overview_ui STATIC)
add_library(QindaQt::ShellGatherOverviewUi ALIAS qindaqt_shell_gather_overview_ui)

qt_add_qml_module(
    qindaqt_shell_gather_overview_ui
    URI QindaQt.Shell.GatherOverview
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml/QindaQt/Shell/GatherOverview"
    TYPEINFO qindaqt_shell_gather_overview_ui.qmltypes
    QML_FILES
        qml/GatherOverviewSurface.qml
        qml/GatherOverviewWindow.qml
        qml/GatherOverviewApplet.qml
        qml/GatherIconChip.qml
        qml/GatherContainerCard.qml
        qml/GatherWindowTile.qml
    DEPENDENCIES
        TARGET qindaqt_shell_icons
        QindaQt.Controls/1.0
        QindaQt.Tokens/1.0
)

target_link_libraries(
    qindaqt_shell_gather_overview_ui
    PUBLIC Qt6::Qml Qt6::Quick
)
target_compile_features(qindaqt_shell_gather_overview_ui PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_shell_gather_overview_ui
    PROPERTIES
        CXX_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE ON
)

if(COMMAND qindaqt_enable_warnings)
    qindaqt_enable_warnings(qindaqt_shell_gather_overview_ui)
endif()
