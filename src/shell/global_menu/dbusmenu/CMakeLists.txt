# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)

qt_add_library(qindaqt_global_menu_dbusmenu STATIC)
add_library(QindaQt::GlobalMenuDbusMenu ALIAS qindaqt_global_menu_dbusmenu)

target_sources(
    qindaqt_global_menu_dbusmenu
    PRIVATE
        src/dbusmenu_client.cpp
        src/dbusmenu_decoder.cpp
        src/dbusmenu_wire.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/global_menu/dbusmenu/dbusmenu_client.h
            include/qindaqt/shell/global_menu/dbusmenu/dbusmenu_decoder.h
            include/qindaqt/shell/global_menu/dbusmenu/dbusmenu_wire.h
)
target_link_libraries(
    qindaqt_global_menu_dbusmenu
    PUBLIC QindaQt::GlobalMenuExporter QindaQt::GlobalMenuProtocol Qt6::Core Qt6::DBus
)
target_compile_features(qindaqt_global_menu_dbusmenu PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_global_menu_dbusmenu PROPERTIES CXX_EXTENSIONS OFF AUTOMOC_PATH_PREFIX ON
)
qindaqt_enable_warnings(qindaqt_global_menu_dbusmenu)

install(
    TARGETS qindaqt_global_menu_dbusmenu
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
