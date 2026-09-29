# SPDX-License-Identifier: LGPL-3.0-or-later
# C is confined to wayland-scanner generated protocol descriptors.
enable_language(C)
find_package(PkgConfig REQUIRED)
pkg_check_modules(QINDAQT_APP_WM_WAYLAND_CLIENT REQUIRED IMPORTED_TARGET GLOBAL wayland-client)
pkg_check_modules(QINDAQT_APP_WM_WAYLAND_SERVER REQUIRED IMPORTED_TARGET GLOBAL wayland-server)
find_program(QINDAQT_APP_WM_SCANNER NAMES wayland-scanner REQUIRED)
set(_app_wm_xml "${CMAKE_CURRENT_SOURCE_DIR}/protocol/qindaqt-window-management-v1.xml")
set(_app_wm_gen "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${_app_wm_gen}")
add_custom_command(OUTPUT "${_app_wm_gen}/protocol.c" "${_app_wm_gen}/qindaqt-window-management-v1-client-protocol.h" "${_app_wm_gen}/qindaqt-window-management-v1-server-protocol.h"
    COMMAND "${QINDAQT_APP_WM_SCANNER}" private-code "${_app_wm_xml}" "${_app_wm_gen}/protocol.c"
    COMMAND "${QINDAQT_APP_WM_SCANNER}" client-header "${_app_wm_xml}" "${_app_wm_gen}/qindaqt-window-management-v1-client-protocol.h"
    COMMAND "${QINDAQT_APP_WM_SCANNER}" server-header "${_app_wm_xml}" "${_app_wm_gen}/qindaqt-window-management-v1-server-protocol.h"
    DEPENDS "${_app_wm_xml}" VERBATIM)
add_library(qindaqt_application_window_protocol STATIC "${_app_wm_gen}/protocol.c")
add_library(QindaQt::ApplicationWindowProtocol ALIAS qindaqt_application_window_protocol)
set_target_properties(qindaqt_application_window_protocol PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(qindaqt_application_window_protocol PUBLIC "$<BUILD_INTERFACE:${_app_wm_gen}>" "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/qindaqt/application_window_management>")
target_link_libraries(qindaqt_application_window_protocol PUBLIC PkgConfig::QINDAQT_APP_WM_WAYLAND_CLIENT)
add_library(qindaqt_application_window_management STATIC src/placement.cpp)
add_library(QindaQt::ApplicationWindowManagement ALIAS qindaqt_application_window_management)
target_sources(qindaqt_application_window_management PUBLIC FILE_SET public_headers TYPE HEADERS
    BASE_DIRS include FILES include/qindaqt/application_window_management/placement.h include/qindaqt/application_window_management/types.h)
target_compile_features(qindaqt_application_window_management PUBLIC cxx_std_20)
target_link_libraries(qindaqt_application_window_management PUBLIC QindaQt::Hybrid)
set_target_properties(qindaqt_application_window_management PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
if(COMMAND qindaqt_enable_warnings)
    qindaqt_enable_warnings(qindaqt_application_window_management)
endif()
install(FILES "${_app_wm_xml}" DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/protocols" COMPONENT WindowManagementSDK)
install(FILES "${_app_wm_gen}/qindaqt-window-management-v1-client-protocol.h" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/qindaqt/application_window_management" COMPONENT WindowManagementSDK)
install(TARGETS qindaqt_application_window_protocol EXPORT QindaQtApplicationWindowManagementTargets ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT WindowManagementSDK)

find_package(Qt6 6.11 REQUIRED COMPONENTS Gui WaylandClient)
find_package(KWayland REQUIRED CONFIG)
add_library(qindaqt_application_window_client SHARED src/client.cpp)
add_library(QindaQt::ApplicationWindowClient ALIAS qindaqt_application_window_client)
target_sources(qindaqt_application_window_client PUBLIC FILE_SET public_headers TYPE HEADERS BASE_DIRS include
    FILES include/qindaqt/application_window_management/client.h include/qindaqt/application_window_management/types.h)
target_compile_features(qindaqt_application_window_client PUBLIC cxx_std_20)
target_link_libraries(qindaqt_application_window_client PUBLIC Qt6::Core Qt6::Gui PRIVATE Qt6::WaylandClient Plasma::KWaylandClient QindaQt::ApplicationWindowProtocol)
set_target_properties(qindaqt_application_window_client PROPERTIES CXX_EXTENSIONS OFF)
if(COMMAND qindaqt_enable_warnings)
    qindaqt_enable_warnings(qindaqt_application_window_client)
endif()
install(TARGETS qindaqt_application_window_client EXPORT QindaQtApplicationWindowManagementTargets LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT WindowManagementSDK FILE_SET public_headers DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}" COMPONENT WindowManagementSDK)

set_target_properties(qindaqt_application_window_client PROPERTIES VERSION 1.0.0 SOVERSION 1)
find_package(Qt6 6.11 REQUIRED COMPONENTS Widgets)
add_executable(qindaqt-window-placement-example examples/main.cpp)
target_compile_features(qindaqt-window-placement-example PRIVATE cxx_std_20)
target_link_libraries(qindaqt-window-placement-example PRIVATE QindaQt::ApplicationWindowClient Qt6::Widgets)
install(TARGETS qindaqt-window-placement-example RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

set_target_properties(qindaqt_application_window_client PROPERTIES EXPORT_NAME ApplicationWindowClient)
set_target_properties(qindaqt_application_window_protocol PROPERTIES EXPORT_NAME ApplicationWindowProtocol)
target_include_directories(qindaqt_application_window_client PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>" "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
include(CMakePackageConfigHelpers)
configure_package_config_file(cmake/QindaQtApplicationWindowManagementConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/QindaQtApplicationWindowManagementConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/QindaQtApplicationWindowManagement")
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/QindaQtApplicationWindowManagementConfigVersion.cmake"
    VERSION 1.0.0 COMPATIBILITY SameMajorVersion)
install(EXPORT QindaQtApplicationWindowManagementTargets NAMESPACE QindaQt::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/QindaQtApplicationWindowManagement" COMPONENT WindowManagementSDK)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/QindaQtApplicationWindowManagementConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/QindaQtApplicationWindowManagementConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/QindaQtApplicationWindowManagement" COMPONENT WindowManagementSDK)
