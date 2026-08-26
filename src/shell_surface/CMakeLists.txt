# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(LayerShellQt 6.6.5 REQUIRED)

qt_add_library(qindaqt_shell_surface STATIC)
add_library(QindaQt::ShellSurface ALIAS qindaqt_shell_surface)

target_sources(
    qindaqt_shell_surface
    PRIVATE
        src/layer_shell_surface_backend.cpp
        src/panel_surface_configuration_planner.cpp
        src/panel_surface_controller.cpp
        src/qt_output_inventory.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell_surface/layer_shell_surface_backend.h
            include/qindaqt/shell_surface/panel_surface_backend.h
            include/qindaqt/shell_surface/panel_surface_configuration.h
            include/qindaqt/shell_surface/panel_surface_configuration_planner.h
            include/qindaqt/shell_surface/panel_surface_controller.h
            include/qindaqt/shell_surface/panel_window_factory.h
            include/qindaqt/shell_surface/qt_output_inventory.h
)

target_link_libraries(
    qindaqt_shell_surface
    PUBLIC
        QindaQt::ShellLayout
        Qt6::Core
    PRIVATE
        LayerShellQt::Interface
        Qt6::Gui
        Qt6::Quick
)
target_compile_features(qindaqt_shell_surface PUBLIC cxx_std_20)
set_target_properties(qindaqt_shell_surface PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_shell_surface)

install(
    TARGETS qindaqt_shell_surface
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
