# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core)

qt_add_library(qindaqt_global_menu_exporter STATIC)
add_library(QindaQt::GlobalMenuExporter ALIAS qindaqt_global_menu_exporter)

target_sources(
    qindaqt_global_menu_exporter
    PRIVATE
        src/menu_exporter.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/global_menu/exporter/menu_exporter.h
            include/qindaqt/shell/global_menu/exporter/menu_source.h
)
target_link_libraries(qindaqt_global_menu_exporter PUBLIC QindaQt::GlobalMenuProtocol Qt6::Core)
target_compile_features(qindaqt_global_menu_exporter PUBLIC cxx_std_20)
set_target_properties(qindaqt_global_menu_exporter PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_global_menu_exporter)

install(
    TARGETS qindaqt_global_menu_exporter
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
