# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)

qt_add_library(qindaqt_link_service STATIC)
add_library(QindaQt::LinkService ALIAS qindaqt_link_service)

target_sources(
    qindaqt_link_service
    PRIVATE
        src/link1_service.cpp
        src/phone_setup.cpp
        src/setup_adaptor.cpp
        src/qt_notification_presenter.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/link_service/link1_service.h
            include/qindaqt/services/link_service/pairing_presenter.h
            include/qindaqt/services/link_service/phone_setup.h
            include/qindaqt/services/link_service/setup_adaptor.h
            include/qindaqt/services/link_service/qt_notification_presenter.h
)
target_link_libraries(
    qindaqt_link_service
    PUBLIC QindaQt::LinkTransport QindaQt::LinkAdb QindaQt::LinkNotifications QindaQt::LinkMessages Qt6::Core Qt6::DBus
)
target_compile_features(qindaqt_link_service PUBLIC cxx_std_20)
set_target_properties(qindaqt_link_service PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_link_service)

qt_add_executable(qindaqt-link-service app/main.cpp)
target_link_libraries(qindaqt-link-service PRIVATE QindaQt::LinkService Qt6::Core Qt6::DBus)
target_compile_definitions(qindaqt-link-service PRIVATE QINDAQT_VERSION="${PROJECT_VERSION}")
set_target_properties(qindaqt-link-service PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt-link-service)

configure_file(
    data/org.qindaqt.Link1.service.in
    "${CMAKE_CURRENT_BINARY_DIR}/org.qindaqt.Link1.service"
    @ONLY
)
configure_file(
    data/qindaqt-link-service.service.in
    "${CMAKE_CURRENT_BINARY_DIR}/qindaqt-link-service.service"
    @ONLY
)

install(
    TARGETS qindaqt_link_service
    EXPORT QindaQtTargets
    ARCHIVE COMPONENT QindaQtLink
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT QindaQtLink
)
install(TARGETS qindaqt-link-service RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT QindaQtLink)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/org.qindaqt.Link1.service"
    DESTINATION "${KDE_INSTALL_DBUSSERVICEDIR}"
    COMPONENT QindaQtLink
)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/qindaqt-link-service.service"
    DESTINATION "${KDE_INSTALL_SYSTEMDUSERUNITDIR}"
    COMPONENT QindaQtLink
)
install(
    FILES data/org.qindaqt.Link1.xml
    DESTINATION "${CMAKE_INSTALL_DATADIR}/dbus-1/interfaces"
    COMPONENT QindaQtLink
)
