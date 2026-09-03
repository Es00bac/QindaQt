# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)

qt_add_library(qindaqt_global_menu_registrar STATIC)
add_library(QindaQt::GlobalMenuRegistrar ALIAS qindaqt_global_menu_registrar)

target_sources(
    qindaqt_global_menu_registrar
    PRIVATE
        src/appmenu_registrar.cpp
        src/appmenu_registrar_object.cpp
        src/appmenu_registrar_object_p.h
        src/qt_bus_credential_source.cpp
        src/registrar_registry.cpp
        src/registrar_wire.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/global_menu/registrar/appmenu_registrar.h
            include/qindaqt/shell/global_menu/registrar/qt_bus_credential_source.h
            include/qindaqt/shell/global_menu/registrar/registrar_registry.h
            include/qindaqt/shell/global_menu/registrar/registrar_wire.h
)
target_link_libraries(
    qindaqt_global_menu_registrar
    PUBLIC QindaQt::GlobalMenuOwnership Qt6::Core Qt6::DBus
)
target_compile_features(qindaqt_global_menu_registrar PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_global_menu_registrar PROPERTIES CXX_EXTENSIONS OFF AUTOMOC_PATH_PREFIX ON
)
qindaqt_enable_warnings(qindaqt_global_menu_registrar)

install(
    TARGETS qindaqt_global_menu_registrar
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
