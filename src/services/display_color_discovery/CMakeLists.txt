# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core)

qt_add_library(qindaqt_display_color_discovery STATIC)
add_library(QindaQt::DisplayColorDiscovery ALIAS qindaqt_display_color_discovery)

target_sources(
    qindaqt_display_color_discovery
    PRIVATE
        src/icc_text_metadata.cpp
        src/import_writer.cpp
        src/profile_discovery.cpp
        src/profile_import.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/display_color_discovery/profile_discovery.h
)
target_link_libraries(qindaqt_display_color_discovery PUBLIC QindaQt::DisplayColorModel Qt6::Core)
target_compile_features(qindaqt_display_color_discovery PUBLIC cxx_std_20)
set_target_properties(qindaqt_display_color_discovery PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_display_color_discovery)

install(
    TARGETS qindaqt_display_color_discovery
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
