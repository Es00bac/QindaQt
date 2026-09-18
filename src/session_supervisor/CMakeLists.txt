# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_library(qindaqt_session_supervisor_support STATIC)
add_library(QindaQt::SessionSupervisorSupport ALIAS qindaqt_session_supervisor_support)
target_sources(
    qindaqt_session_supervisor_support
    PRIVATE
        src/activation_environment.cpp
        src/activation_environment.h
        src/direct_parent_process.cpp
        src/first_launch_welcome.cpp
        src/first_launch_welcome.h
        src/session_optional_child.cpp
        src/polkit_agent_selection.cpp
        src/resident_service_refresh.cpp
        src/resident_service_refresh.h
        src/systemd_manager_port.cpp
        src/systemd_manager_port.h
        src/supervised_process_launcher.cpp
        src/tokenized_process_launcher.cpp
        src/session_process_supervisor.cpp
        src/session_service.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/session_supervisor/direct_parent_process.h
            include/qindaqt/session_supervisor/session_optional_child.h
            include/qindaqt/session_supervisor/polkit_agent_selection.h
            include/qindaqt/session_supervisor/supervised_process_launcher.h
            include/qindaqt/session_supervisor/tokenized_process_launcher.h
            include/qindaqt/session_supervisor/session_process_supervisor.h
            include/qindaqt/session_supervisor/session_service.h
)
# sd-bus reaches the systemd user manager's private control endpoint, which
# is not a message bus and never answers Hello (see systemd_manager_port.h).
find_package(PkgConfig REQUIRED)
pkg_check_modules(libsystemd REQUIRED IMPORTED_TARGET libsystemd)

target_link_libraries(
    qindaqt_session_supervisor_support
    PUBLIC QindaQt::NotificationPresentationProtocol Qt6::Core Qt6::DBus
    PRIVATE PkgConfig::libsystemd
)
target_compile_features(qindaqt_session_supervisor_support PUBLIC cxx_std_20)
set_target_properties(
    qindaqt_session_supervisor_support PROPERTIES CXX_EXTENSIONS OFF
)
qindaqt_enable_warnings(qindaqt_session_supervisor_support)

qt_add_executable(qindaqt-session app/main.cpp)
target_link_libraries(
    qindaqt-session
    PRIVATE
        QindaQt::SessionSupervisorSupport
        QindaQt::WindowManagementBridge
        Qt6::Core
        Qt6::DBus
)
target_compile_definitions(qindaqt-session PRIVATE QINDAQT_VERSION="${PROJECT_VERSION}")
set_target_properties(qindaqt-session PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt-session)

install(
    TARGETS qindaqt_session_supervisor_support
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
install(TARGETS qindaqt-session RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(
    FILES org.qindaqt.Session1.xml
    DESTINATION "${CMAKE_INSTALL_DATADIR}/dbus-1/interfaces"
)
