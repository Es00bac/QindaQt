# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS DBus)

qt_add_library(qindaqt_session_lock_state STATIC)
add_library(QindaQt::SessionLockState ALIAS qindaqt_session_lock_state)

target_sources(
    qindaqt_session_lock_state
    PRIVATE
        src/qt_session_lock_transport.cpp
        src/session_lock_state.cpp
        src/session_lock_state_monitor.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/session_lock_state/qt_session_lock_transport.h
            include/qindaqt/services/session_lock_state/session_lock_state.h
            include/qindaqt/services/session_lock_state/session_lock_state_monitor.h
            include/qindaqt/services/session_lock_state/session_lock_transport.h
)
target_link_libraries(qindaqt_session_lock_state PUBLIC Qt6::Core Qt6::DBus)
target_compile_features(qindaqt_session_lock_state PUBLIC cxx_std_20)
set_target_properties(qindaqt_session_lock_state PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_session_lock_state)

install(
    TARGETS qindaqt_session_lock_state
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
