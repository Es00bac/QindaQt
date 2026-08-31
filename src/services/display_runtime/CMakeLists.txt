# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)

qt_add_library(qindaqt_display_runtime STATIC)
add_library(QindaQt::DisplayRuntime ALIAS qindaqt_display_runtime)
target_sources(
    qindaqt_display_runtime
    PRIVATE
        src/qt_session_safety_port.cpp
        src/resident_display_runtime.cpp
        src/state_root.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/display_runtime/resident_display_runtime.h
            include/qindaqt/services/display_runtime/session_safety_port.h
            include/qindaqt/services/display_runtime/state_root.h
)
target_link_libraries(
    qindaqt_display_runtime
    PUBLIC
        QindaQt::DisplayService
        QindaQt::DisplayTransaction
        QindaQt::DisplayWriter
        Qt6::Core
        Qt6::DBus
    PRIVATE QindaQt::SessionLockState
)
target_compile_features(qindaqt_display_runtime PUBLIC cxx_std_20)
set_target_properties(qindaqt_display_runtime PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_display_runtime)

install(
    TARGETS qindaqt_display_runtime
    EXPORT QindaQtTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT DisplayRuntimeDevelopment
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        COMPONENT DisplayRuntimeDevelopment
)

qt_add_executable(qindaqt-display-service app/main.cpp)
target_link_libraries(
    qindaqt-display-service
    PRIVATE QindaQt::DisplayJournal QindaQt::DisplayRuntime Qt6::Core Qt6::DBus
)
target_compile_definitions(
    qindaqt-display-service PRIVATE QINDAQT_VERSION="${PROJECT_VERSION}"
)
set_target_properties(qindaqt-display-service PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt-display-service)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../display_service/data/org.qindaqt.Display1.service.in"
    "${CMAKE_CURRENT_BINARY_DIR}/org.qindaqt.Display1.service"
    @ONLY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../display_service/data/qindaqt-display-service.service.in"
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
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/../display_service/data/org.qindaqt.Display1.xml"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/dbus-1/interfaces"
)
