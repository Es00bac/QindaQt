# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(WirePlumber REQUIRED IMPORTED_TARGET wireplumber-0.5>=0.5)
# The console realises each matrix send as a libpipewire-module-loopback, which
# needs PipeWire's own module API; WirePlumber wraps the graph but does not
# expose module loading (ADR-0173).
pkg_check_modules(PipeWire REQUIRED IMPORTED_TARGET libpipewire-0.3>=1.0)

# The recorder (ADR-0184) writes FLAC and WAV through libsndfile.
pkg_check_modules(SNDFILE REQUIRED IMPORTED_TARGET sndfile)

qt_add_library(qindaqt_audio_service STATIC)
add_library(QindaQt::AudioService ALIAS qindaqt_audio_service)

target_sources(
    qindaqt_audio_service
    PRIVATE
        src/audio_operation_admission.cpp
        src/audio_operation_admission_p.h
        src/audio_operation_coordinator.cpp
        src/audio_service_object.cpp
        src/audio_service_object_p.h
        src/resident_audio_service.cpp
        src/audio_console_operations.cpp
        src/audio_vban_operations.cpp
        src/console_endpoints.cpp
        src/console_endpoints_p.h
        src/console_model.cpp
        src/console_store.cpp
        src/macro_store.cpp
        src/preset_store.cpp
        src/wireplumber_audio_backend.cpp
        src/wireplumber_graph.cpp
        src/wireplumber_meters.cpp
        src/wireplumber_meters_p.h
        src/wireplumber_processing.cpp
        src/wireplumber_processing_p.h
        src/wireplumber_routing.cpp
        src/wireplumber_routing_p.h
        src/wireplumber_graph_p.h
        src/wireplumber_worker.cpp
        src/wireplumber_worker_endpoints.cpp
        src/wireplumber_worker_metering.cpp
        src/wireplumber_worker_modules.cpp
        src/wireplumber_worker_processing.cpp
        src/wireplumber_worker_recording.cpp
        src/wireplumber_worker_vban.cpp
        src/wireplumber_vban.cpp
        src/wireplumber_vban_p.h
        src/vban_packet.cpp
        src/vban_store.cpp
        src/vban_packet_p.h
        src/wireplumber_recorder.cpp
        src/wireplumber_recorder_p.h
        src/wireplumber_worker_operations.cpp
        src/wireplumber_worker_routing.cpp
        src/wireplumber_worker_p.h
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/audio_service/audio_backend.h
            include/qindaqt/services/audio_service/console_model.h
            include/qindaqt/services/audio_service/console_store.h
            include/qindaqt/services/audio_service/macro_store.h
            include/qindaqt/services/audio_service/preset_store.h
            include/qindaqt/services/audio_service/vban_store.h
            include/qindaqt/services/audio_service/audio_operation_coordinator.h
            include/qindaqt/services/audio_service/resident_audio_service.h
            include/qindaqt/services/audio_service/wireplumber_audio_backend.h
)
target_link_libraries(
    qindaqt_audio_service
    PUBLIC QindaQt::AudioProtocol Qt6::Core Qt6::DBus
    PRIVATE PkgConfig::WirePlumber PkgConfig::PipeWire PkgConfig::SNDFILE Threads::Threads
)
target_compile_features(qindaqt_audio_service PUBLIC cxx_std_20)
target_compile_definitions(qindaqt_audio_service PRIVATE QT_NO_KEYWORDS)
set_target_properties(qindaqt_audio_service PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_audio_service)

install(
    TARGETS qindaqt_audio_service
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

qt_add_executable(qindaqt-audio-service app/main.cpp)
target_link_libraries(
    qindaqt-audio-service PRIVATE QindaQt::AudioService Qt6::Core Qt6::DBus
)
target_compile_definitions(
    qindaqt-audio-service PRIVATE QINDAQT_VERSION="${PROJECT_VERSION}"
)
set_target_properties(qindaqt-audio-service PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt-audio-service)

configure_file(
    data/org.qindaqt.Audio1.service.in
    "${CMAKE_CURRENT_BINARY_DIR}/org.qindaqt.Audio1.service"
    @ONLY
)
configure_file(
    data/qindaqt-audio-service.service.in
    "${CMAKE_CURRENT_BINARY_DIR}/qindaqt-audio-service.service"
    @ONLY
)

install(TARGETS qindaqt-audio-service RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/org.qindaqt.Audio1.service"
    DESTINATION "${KDE_INSTALL_DBUSSERVICEDIR}"
)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/qindaqt-audio-service.service"
    DESTINATION "${KDE_INSTALL_SYSTEMDUSERUNITDIR}"
)
install(
    FILES data/org.qindaqt.Audio1.xml
    DESTINATION "${CMAKE_INSTALL_DATADIR}/dbus-1/interfaces"
)
