# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)

qt_add_library(qindaqt_power_service STATIC)
add_library(QindaQt::PowerService ALIAS qindaqt_power_service)

target_sources(
    qindaqt_power_service
    PRIVATE
        src/power_service_assembly.cpp
        src/power_service_assembly_p.h
        src/power_service_coordinator.cpp
        src/power_service_object.cpp
        src/power_service_object_p.h
        src/power_service_operations.cpp
        src/resident_power_service.cpp
        src/unavailable_power_collaborators.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/power_service/power_collaborators.h
            include/qindaqt/services/power_service/power_service_coordinator.h
            include/qindaqt/services/power_service/resident_power_service.h
            include/qindaqt/services/power_service/unavailable_power_collaborators.h
)
target_link_libraries(
    qindaqt_power_service
    PUBLIC QindaQt::PowerProtocol Qt6::Core Qt6::DBus
)
target_compile_features(qindaqt_power_service PUBLIC cxx_std_20)
target_compile_definitions(qindaqt_power_service PRIVATE QT_NO_KEYWORDS)
set_target_properties(qindaqt_power_service PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_power_service)

install(
    TARGETS qindaqt_power_service
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

qt_add_executable(qindaqt-power-service app/main.cpp)
target_link_libraries(
    qindaqt-power-service PRIVATE QindaQt::PowerService Qt6::Core Qt6::DBus
)
target_compile_definitions(
    qindaqt-power-service PRIVATE QINDAQT_VERSION="${PROJECT_VERSION}"
)
set_target_properties(qindaqt-power-service PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt-power-service)

configure_file(
    data/org.qindaqt.Power1.service.in
    "${CMAKE_CURRENT_BINARY_DIR}/org.qindaqt.Power1.service"
    @ONLY
)
configure_file(
    data/qindaqt-power-service.service.in
    "${CMAKE_CURRENT_BINARY_DIR}/qindaqt-power-service.service"
    @ONLY
)

install(TARGETS qindaqt-power-service RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/org.qindaqt.Power1.service"
    DESTINATION "${KDE_INSTALL_DBUSSERVICEDIR}"
)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/qindaqt-power-service.service"
    DESTINATION "${KDE_INSTALL_SYSTEMDUSERUNITDIR}"
)
install(
    FILES data/org.qindaqt.Power1.xml
    DESTINATION "${CMAKE_INSTALL_DATADIR}/dbus-1/interfaces"
)
