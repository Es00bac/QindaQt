# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core)

qt_add_library(qindaqt_global_menu_protocol STATIC)
add_library(QindaQt::GlobalMenuProtocol ALIAS qindaqt_global_menu_protocol)

target_sources(
    qindaqt_global_menu_protocol
    PRIVATE
        src/menu_delta.cpp
        src/menu_item_lookup.cpp
        src/menu_validation.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/shell/global_menu/protocol/menu_delta.h
            include/qindaqt/shell/global_menu/protocol/menu_item.h
            include/qindaqt/shell/global_menu/protocol/menu_item_lookup.h
            include/qindaqt/shell/global_menu/protocol/menu_limits.h
            include/qindaqt/shell/global_menu/protocol/menu_tree.h
            include/qindaqt/shell/global_menu/protocol/menu_validation.h
)
target_link_libraries(qindaqt_global_menu_protocol PUBLIC Qt6::Core)
target_compile_features(qindaqt_global_menu_protocol PUBLIC cxx_std_20)
set_target_properties(qindaqt_global_menu_protocol PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_global_menu_protocol)

install(
    TARGETS qindaqt_global_menu_protocol
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
