# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(KF6Config 6.0 REQUIRED CONFIG)

add_library(qindaqt_powerdevil_idle STATIC
    src/powerdevil_idle_adapter.cpp
)
add_library(QindaQt::PowerDevilIdle ALIAS qindaqt_powerdevil_idle)
target_sources(
    qindaqt_powerdevil_idle
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES include/qindaqt/session/powerdevil_idle/powerdevil_idle_adapter.h
)

target_compile_features(qindaqt_powerdevil_idle PUBLIC cxx_std_20)
target_include_directories(
    qindaqt_powerdevil_idle
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_link_libraries(
    qindaqt_powerdevil_idle
    PUBLIC
        Qt6::Core
        Qt6::DBus
        KF6::ConfigCore
)
set_target_properties(qindaqt_powerdevil_idle PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_powerdevil_idle)

install(
    TARGETS qindaqt_powerdevil_idle
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
