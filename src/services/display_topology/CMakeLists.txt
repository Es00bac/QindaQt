# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core)

qt_add_library(qindaqt_display_topology STATIC)
add_library(QindaQt::DisplayTopology ALIAS qindaqt_display_topology)

target_sources(
    qindaqt_display_topology
    PRIVATE
        src/topology_fingerprint.cpp
        src/topology_geometry.cpp
        src/topology_validation.cpp
        src/topology_validation_p.h
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/display_topology/topology.h
            include/qindaqt/services/display_topology/topology_types.h
)
target_link_libraries(qindaqt_display_topology PUBLIC QindaQt::DisplayProtocol Qt6::Core)
target_compile_features(qindaqt_display_topology PUBLIC cxx_std_20)
set_target_properties(qindaqt_display_topology PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_display_topology)

install(
    TARGETS qindaqt_display_topology
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
