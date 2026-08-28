# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)

qt_add_library(qindaqt_display_service STATIC)
add_library(QindaQt::DisplayService ALIAS qindaqt_display_service)
target_sources(
    qindaqt_display_service
    PRIVATE
        src/compositor_inventory_source.cpp
        src/display_inventory_codec.cpp
        src/display_inventory_projection.cpp
        src/display_inventory_validation_p.h
        src/display_service_model.cpp
        src/display_service_object.cpp
        src/display_service_object_p.h
        src/display_service_projection_p.h
        src/resident_display_service.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/display_service/display_inventory.h
            include/qindaqt/services/display_service/display_service_model.h
            include/qindaqt/services/display_service/display_service_ports.h
            include/qindaqt/services/display_service/resident_display_service.h
)
target_link_libraries(
    qindaqt_display_service
    PUBLIC
        QindaQt::DisplayIdentity
        QindaQt::DisplayProtocol
        QindaQt::DisplayTopology
        QindaQt::DisplayTransaction
        Qt6::Core
        Qt6::DBus
)
target_compile_features(qindaqt_display_service PUBLIC cxx_std_20)
target_compile_definitions(qindaqt_display_service PRIVATE QT_NO_KEYWORDS)
set_target_properties(qindaqt_display_service PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_display_service)

install(
    TARGETS qindaqt_display_service
    EXPORT QindaQtTargets
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

qt_add_executable(qindaqt-display-service app/main.cpp)
target_link_libraries(
    qindaqt-display-service PRIVATE QindaQt::DisplayService Qt6::Core Qt6::DBus
)
target_compile_definitions(
    qindaqt-display-service PRIVATE QINDAQT_VERSION="${PROJECT_VERSION}"
)
set_target_properties(qindaqt-display-service PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt-display-service)

configure_file(
    data/org.qindaqt.Display1.service.in
    "${CMAKE_CURRENT_BINARY_DIR}/org.qindaqt.Display1.service"
    @ONLY
)
configure_file(
    data/qindaqt-display-service.service.in
    "${CMAKE_CURRENT_BINARY_DIR}/qindaqt-display-service.service"
    @ONLY
)

install(TARGETS qindaqt-display-service RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/org.qindaqt.Display1.service"
    DESTINATION "${KDE_INSTALL_DBUSSERVICEDIR}"
)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/qindaqt-display-service.service"
    DESTINATION "${KDE_INSTALL_SYSTEMDUSERUNITDIR}"
)
install(
    FILES data/org.qindaqt.Display1.xml
    DESTINATION "${CMAKE_INSTALL_DATADIR}/dbus-1/interfaces"
)
