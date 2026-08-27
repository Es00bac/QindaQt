# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(WirePlumber REQUIRED IMPORTED_TARGET wireplumber-0.5>=0.5)

qt_add_library(qindaqt_audio_service STATIC)
add_library(QindaQt::AudioService ALIAS qindaqt_audio_service)

target_sources(
    qindaqt_audio_service
    PRIVATE
        src/audio_operation_coordinator.cpp
        src/audio_service_object.cpp
        src/audio_service_object_p.h
        src/resident_audio_service.cpp
        src/wireplumber_audio_backend.cpp
        src/wireplumber_graph.cpp
        src/wireplumber_graph_p.h
        src/wireplumber_worker.cpp
        src/wireplumber_worker_operations.cpp
        src/wireplumber_worker_p.h
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/audio_service/audio_backend.h
            include/qindaqt/services/audio_service/audio_operation_coordinator.h
            include/qindaqt/services/audio_service/resident_audio_service.h
            include/qindaqt/services/audio_service/wireplumber_audio_backend.h
)
target_link_libraries(
    qindaqt_audio_service
    PUBLIC QindaQt::AudioProtocol Qt6::Core Qt6::DBus
    PRIVATE PkgConfig::WirePlumber Threads::Threads
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
