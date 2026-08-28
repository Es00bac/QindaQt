# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core WaylandClient)
find_package(Wayland REQUIRED COMPONENTS Client)

qt_add_library(qindaqt_display_writer STATIC)
add_library(QindaQt::DisplayWriter ALIAS qindaqt_display_writer)

qt6_generate_wayland_protocol_client_sources(
    qindaqt_display_writer
    FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/protocol/kde-output-device-v2.xml"
        "${CMAKE_CURRENT_SOURCE_DIR}/protocol/kde-output-management-v2.xml"
    PRIVATE_CODE
)
# Qt 6.11's generated registry init accepts an int and forwards it to
# libwayland's uint32_t API. Isolate that generator-owned conversion rather
# than weakening warnings for hand-written writer code.
set_source_files_properties(
    "${CMAKE_CURRENT_BINARY_DIR}/qwayland-kde-output-device-v2.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/qwayland-kde-output-management-v2.cpp"
    PROPERTIES COMPILE_OPTIONS "-Wno-sign-conversion;-Wno-shadow"
)

target_sources(
    qindaqt_display_writer
    PRIVATE
        src/apply_request_mapper.cpp
        src/configuration_validation.cpp
        src/qt_wayland_output_objects_p.h
        src/qt_wayland_output_management_port.cpp
        src/writer_transaction_port.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/display_writer/output_management_port.h
            include/qindaqt/services/display_writer/production_output_management_port.h
            include/qindaqt/services/display_writer/writer_transaction_port.h
)

target_link_libraries(
    qindaqt_display_writer
    PUBLIC
        QindaQt::DisplayService
        Qt6::Core
    PRIVATE
        Qt6::WaylandClient
        Wayland::Client
)
target_compile_features(qindaqt_display_writer PUBLIC cxx_std_20)
set_target_properties(qindaqt_display_writer PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_display_writer)

install(
    TARGETS qindaqt_display_writer
    EXPORT QindaQtTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT DisplayWriterDevelopment
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        COMPONENT DisplayWriterDevelopment
)
