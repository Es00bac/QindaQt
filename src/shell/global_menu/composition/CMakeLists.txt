# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)

qt_add_library(qindaqt_global_menu_transport_composition STATIC)
add_library(
    QindaQt::GlobalMenuTransportComposition ALIAS qindaqt_global_menu_transport_composition
)

target_sources(
    qindaqt_global_menu_transport_composition
    PRIVATE src/global_menu_transport_coordinator.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/global_menu/composition/announced_menu_address_source.h
            include/qindaqt/shell/global_menu/composition/global_menu_transport_coordinator.h
            include/qindaqt/shell/global_menu/composition/registrar_window_id_source.h
)
target_link_libraries(
    qindaqt_global_menu_transport_composition
    PUBLIC
        QindaQt::GlobalMenuApplet
        QindaQt::GlobalMenuDbusMenu
        QindaQt::GlobalMenuExporter
        QindaQt::GlobalMenuOwnership
        QindaQt::GlobalMenuRegistrar
        Qt6::Core
        Qt6::DBus
)
target_compile_features(qindaqt_global_menu_transport_composition PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_global_menu_transport_composition PROPERTIES CXX_EXTENSIONS OFF AUTOMOC_PATH_PREFIX ON
)
qindaqt_enable_warnings(qindaqt_global_menu_transport_composition)

install(
    TARGETS qindaqt_global_menu_transport_composition
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
