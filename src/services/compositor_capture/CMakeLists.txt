# SPDX-License-Identifier: GPL-3.0-or-later
# Cohesive public capture boundary shared by Screenshot and portal helpers.
qt_add_library(qindaqt_compositor_capture STATIC)
add_library(QindaQt::CompositorCapture ALIAS qindaqt_compositor_capture)
target_sources(qindaqt_compositor_capture PRIVATE
    src/capture_request.cpp src/raw_capture_decoder.cpp src/region_geometry.cpp src/kwin_capture_port.cpp
    PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
    include/qindaqt/services/compositor_capture/capture_request.h
    include/qindaqt/services/compositor_capture/raw_capture_decoder.h
    include/qindaqt/services/compositor_capture/region_geometry.h
    include/qindaqt/services/compositor_capture/capture_port.h
    include/qindaqt/services/compositor_capture/kwin_capture_port.h)
target_link_libraries(qindaqt_compositor_capture PUBLIC Qt6::Core Qt6::Gui Qt6::DBus QindaQt::CompositorNames)
target_compile_features(qindaqt_compositor_capture PUBLIC cxx_std_20)
qindaqt_enable_warnings(qindaqt_compositor_capture)
install(TARGETS qindaqt_compositor_capture EXPORT QindaQtTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT QindaQtPortalP0
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}" COMPONENT QindaQtPortalP0)

find_package(PkgConfig REQUIRED)
pkg_check_modules(CAPTURE_WAYLAND REQUIRED IMPORTED_TARGET wayland-client)
find_program(CAPTURE_WAYLAND_SCANNER wayland-scanner REQUIRED)
find_file(CAPTURE_SCREENCAST_XML zkde-screencast-unstable-v1.xml PATHS /usr/share/plasma-wayland-protocols REQUIRED)
enable_language(C)
add_custom_command(OUTPUT screencast-client.h screencast-protocol.c
    COMMAND "${CAPTURE_WAYLAND_SCANNER}" client-header "${CAPTURE_SCREENCAST_XML}" screencast-client.h
    COMMAND "${CAPTURE_WAYLAND_SCANNER}" private-code "${CAPTURE_SCREENCAST_XML}" screencast-protocol.c
    DEPENDS "${CAPTURE_SCREENCAST_XML}" VERBATIM)
qt_add_library(qindaqt_wayland_screencast STATIC src/wayland_screencast.cpp screencast-client.h screencast-protocol.c)
add_library(QindaQt::WaylandScreenCast ALIAS qindaqt_wayland_screencast)
target_sources(qindaqt_wayland_screencast PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
    include/qindaqt/services/compositor_capture/wayland_screencast.h)
target_include_directories(qindaqt_wayland_screencast PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(qindaqt_wayland_screencast PUBLIC Qt6::Core PRIVATE PkgConfig::CAPTURE_WAYLAND)
target_compile_features(qindaqt_wayland_screencast PUBLIC cxx_std_20)
set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/screencast-protocol.c" PROPERTIES COMPILE_OPTIONS "-w")
qindaqt_enable_warnings(qindaqt_wayland_screencast)
install(TARGETS qindaqt_wayland_screencast EXPORT QindaQtTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT QindaQtPortalP0
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}" COMPONENT QindaQtPortalP0)
