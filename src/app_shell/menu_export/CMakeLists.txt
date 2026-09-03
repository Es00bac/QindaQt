# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus Gui)
find_package(Qt6GuiPrivate 6.11 REQUIRED)

qt_add_library(qindaqt_app_shell_menu_export STATIC)
add_library(QindaQt::AppShellMenuExport ALIAS qindaqt_app_shell_menu_export)

target_sources(
    qindaqt_app_shell_menu_export
    PRIVATE
        src/application_menu_export.cpp
        src/qt_window_menu_identity.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/app_shell/menu_export/application_menu_export.h
            include/qindaqt/app_shell/menu_export/window_menu_identity.h
)
target_link_libraries(
    qindaqt_app_shell_menu_export
    PUBLIC QindaQt::AppShell Qt6::Core Qt6::DBus Qt6::Gui
    PRIVATE
        QindaQt::GlobalMenuDbusMenu
        QindaQt::GlobalMenuProtocol
        QindaQt::GlobalMenuRegistrar
)
target_include_directories(
    qindaqt_app_shell_menu_export
    SYSTEM PRIVATE
        "$<TARGET_PROPERTY:Qt6::GuiPrivate,INTERFACE_INCLUDE_DIRECTORIES>"
        "$<TARGET_PROPERTY:Qt6::CorePrivate,INTERFACE_INCLUDE_DIRECTORIES>"
)
target_compile_features(qindaqt_app_shell_menu_export PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_app_shell_menu_export
    PROPERTIES CXX_EXTENSIONS OFF AUTOMOC_PATH_PREFIX ON
)
qindaqt_enable_warnings(qindaqt_app_shell_menu_export)

install(
    TARGETS qindaqt_app_shell_menu_export
    EXPORT QindaQtTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
