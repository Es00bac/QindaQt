# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus Gui)

qt_add_library(qindaqt_link_macros STATIC)
add_library(QindaQt::LinkMacros ALIAS qindaqt_link_macros)

target_sources(
    qindaqt_link_macros
    PRIVATE
        src/keysym_map.cpp
        src/macro_catalog.cpp
        src/macro_feature.cpp
        src/portal_macro_output.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/link_macros/keysym_map.h
            include/qindaqt/services/link_macros/macro_catalog.h
            include/qindaqt/services/link_macros/macro_feature.h
            include/qindaqt/services/link_macros/portal_macro_output.h
)
target_link_libraries(
    qindaqt_link_macros
    PUBLIC QindaQt::LinkInput QindaQt::InputActions Qt6::Core Qt6::DBus Qt6::Gui
)
target_compile_features(qindaqt_link_macros PUBLIC cxx_std_20)
set_target_properties(qindaqt_link_macros PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_link_macros)
