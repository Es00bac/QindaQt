# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(KF6Config 6.0 REQUIRED CONFIG)

add_library(qindaqt_powerdevil_profile STATIC
    src/powerdevil_profile_adapter.cpp
)
add_library(QindaQt::PowerDevilProfile ALIAS qindaqt_powerdevil_profile)
target_sources(
    qindaqt_powerdevil_profile
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES include/qindaqt/session/powerdevil_profile/powerdevil_profile_adapter.h
)

target_compile_features(qindaqt_powerdevil_profile PUBLIC cxx_std_20)
target_include_directories(
    qindaqt_powerdevil_profile
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_link_libraries(
    qindaqt_powerdevil_profile
    PUBLIC
        Qt6::Core
        Qt6::DBus
        KF6::ConfigCore
)
set_target_properties(qindaqt_powerdevil_profile PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_powerdevil_profile)

install(
    TARGETS qindaqt_powerdevil_profile
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
