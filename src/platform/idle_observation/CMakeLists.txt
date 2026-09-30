# SPDX-License-Identifier: GPL-3.0-or-later
find_package(Wayland REQUIRED COMPONENTS Client)
find_package(PkgConfig REQUIRED)
pkg_get_variable(QINDAQT_IDLE_PROTOCOL_DATA wayland-protocols pkgdatadir)
find_program(QINDAQT_IDLE_SCANNER wayland-scanner REQUIRED)
set(QINDAQT_IDLE_XML "${QINDAQT_IDLE_PROTOCOL_DATA}/staging/ext-idle-notify/ext-idle-notify-v1.xml")
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/ext-idle-notify-client.h" "${CMAKE_CURRENT_BINARY_DIR}/ext-idle-notify-protocol.c"
    COMMAND ${QINDAQT_IDLE_SCANNER} client-header ${QINDAQT_IDLE_XML} "${CMAKE_CURRENT_BINARY_DIR}/ext-idle-notify-client.h"
    COMMAND ${QINDAQT_IDLE_SCANNER} private-code ${QINDAQT_IDLE_XML} "${CMAKE_CURRENT_BINARY_DIR}/ext-idle-notify-protocol.c"
    DEPENDS ${QINDAQT_IDLE_XML} VERBATIM)
enable_language(C)
qt_add_library(qindaqt_idle_observation STATIC)
add_library(QindaQt::IdleObservation ALIAS qindaqt_idle_observation)
target_sources(qindaqt_idle_observation PRIVATE src/wayland_idle_observation.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/ext-idle-notify-client.h" "${CMAKE_CURRENT_BINARY_DIR}/ext-idle-notify-protocol.c"
    PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
    include/qindaqt/platform/idle_observation/idle_observation.h)
target_include_directories(qindaqt_idle_observation PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(qindaqt_idle_observation PUBLIC Qt6::Core PRIVATE Wayland::Client)
target_compile_features(qindaqt_idle_observation PUBLIC cxx_std_20)
set_target_properties(qindaqt_idle_observation PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_idle_observation)
install(TARGETS qindaqt_idle_observation EXPORT QindaQtTargets
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
