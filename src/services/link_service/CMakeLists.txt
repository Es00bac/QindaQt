# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)

qt_add_library(qindaqt_link_service STATIC)
add_library(QindaQt::LinkService ALIAS qindaqt_link_service)

target_sources(
    qindaqt_link_service
    PRIVATE
        src/avahi_publisher.cpp
        src/caller_policy.cpp
        src/lan_controller.cpp
        src/lan_settings.cpp
        src/link1_service.cpp
        src/phone_setup.cpp
        src/setup_adaptor.cpp
        src/qt_notification_presenter.cpp
        src/send_guard.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/link_service/avahi_publisher.h
            include/qindaqt/services/link_service/caller_policy.h
            include/qindaqt/services/link_service/lan_controller.h
            include/qindaqt/services/link_service/lan_settings.h
            include/qindaqt/services/link_service/link1_service.h
            include/qindaqt/services/link_service/pairing_presenter.h
            include/qindaqt/services/link_service/phone_setup.h
            include/qindaqt/services/link_service/setup_adaptor.h
            include/qindaqt/services/link_service/qt_notification_presenter.h
            include/qindaqt/services/link_service/send_guard.h
            include/qindaqt/services/link_service/service_publisher.h
)
target_link_libraries(
    qindaqt_link_service
    PUBLIC QindaQt::LinkTransport QindaQt::LinkAdb QindaQt::LinkNotifications QindaQt::LinkMessages Qt6::Core Qt6::DBus
)
target_compile_features(qindaqt_link_service PUBLIC cxx_std_20)
set_target_properties(qindaqt_link_service PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_link_service)

# AGENT-NOTE: input and macros are composed in the executable only. Their
# libraries (and InputActions) are not part of the installed export set.
qt_add_executable(
    qindaqt-link-service
    app/main.cpp
    app/input_composition.cpp
    app/input_composition.h
    app/native_input_gate.cpp
    app/native_input_gate.h
)
target_link_libraries(
    qindaqt-link-service
    PRIVATE
        QindaQt::LinkService
        QindaQt::LinkInput
        QindaQt::LinkMacros
        QindaQt::SessionLockState
        QindaQt::CompositorAttachment
        qindaqt_compositor_names
        Qt6::Core
        Qt6::DBus
)
target_include_directories(qindaqt-link-service PRIVATE app)
target_compile_definitions(
    qindaqt-link-service
    PRIVATE QINDAQT_VERSION="${PROJECT_VERSION}" QINDAQT_BINDIR="${KDE_INSTALL_FULL_BINDIR}"
)
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
