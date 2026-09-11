# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(KF6Config 6.0 REQUIRED CONFIG)

add_library(qindaqt_powerdevil_lid STATIC
    src/powerdevil_lid_adapter.cpp
)
add_library(QindaQt::PowerDevilLid ALIAS qindaqt_powerdevil_lid)
target_sources(
    qindaqt_powerdevil_lid
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES include/qindaqt/session/powerdevil_lid/powerdevil_lid_adapter.h
)

target_compile_features(qindaqt_powerdevil_lid PUBLIC cxx_std_20)
target_include_directories(
    qindaqt_powerdevil_lid
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_link_libraries(
    qindaqt_powerdevil_lid
    PUBLIC
        Qt6::Core
        Qt6::DBus
        KF6::ConfigCore
)
set_target_properties(qindaqt_powerdevil_lid PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_powerdevil_lid)

install(
    TARGETS qindaqt_powerdevil_lid
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
