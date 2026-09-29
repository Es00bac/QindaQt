# SPDX-License-Identifier: GPL-3.0-or-later
find_package(QindaTK CONFIG REQUIRED)
find_package(Qt6 6.11 REQUIRED COMPONENTS Quick Qml QuickControls2 GuiPrivate)
if(NOT TARGET qindaqt_osk_core)
    message(FATAL_ERROR "Native locker requires the first-party pure OSK model")
endif()
set(_lock_greeter_sources authentication_controller.cpp authentication_controller.h
    touch_keyboard.cpp touch_keyboard.h saver_presentation.cpp saver_presentation.h
    native_windows.cpp native_windows.h)
function(qindaqt_lock_greeter_common target)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(${target} PRIVATE qindaqt_lock_protocol qindaqt_lock_platform
        qindaqt_lock_worker_wire qindaqt_osk_core QindaQt::DesktopControls
        QindaTK::qindatk Qt6::GuiPrivate Qt6::Quick Qt6::Qml Qt6::QuickControls2)
    qt_add_resources(${target} native_lock_ui PREFIX "/native-lock"
        FILES Main.qml PatrolScene.qml ReefScene.qml)
    qindaqt_enable_warnings(${target})
endfunction()
# Production object code always builds: its worker has no fixture constructors.
qt_add_executable(qindaqt-lock main.cpp ${_lock_greeter_sources})
qindaqt_lock_greeter_common(qindaqt-lock)
target_link_libraries(qindaqt-lock PRIVATE qindaqt_lock_worker_client)
add_dependencies(qindaqt-lock qindaqt_session_lock_integration)
install(TARGETS qindaqt-lock RUNTIME DESTINATION "${KDE_INSTALL_BINDIR}" COMPONENT Shell)
configure_file(org.qindaqt.Lock.desktop.in org.qindaqt.Lock.desktop @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/org.qindaqt.Lock.desktop"
    DESTINATION "${KDE_INSTALL_DATADIR}/applications" COMPONENT Shell)
if(BUILD_TESTING)
    qt_add_executable(qindaqt-private-lock-greeter tests/private_greeter.cpp ${_lock_greeter_sources}
        ../lock_worker_client/worker_process.cpp ../lock_worker_client/worker_process.h)
    qindaqt_lock_greeter_common(qindaqt-private-lock-greeter)
    target_include_directories(qindaqt-private-lock-greeter PRIVATE ../lock_worker_client)
    target_compile_definitions(qindaqt-private-lock-greeter PRIVATE QINDAQT_PRIVATE_WORKER_CLIENT_FIXTURE
        QINDAQT_LOCK_PAM_EXECUTABLE="${KDE_INSTALL_FULL_LIBEXECDIR}/qindaqt-lock-pam"
        QINDAQT_PRIVATE_LOCK_PLUGIN_ROOT="${CMAKE_BINARY_DIR}/src/lock_protocol/plugins"
        QINDAQT_PRIVATE_WORKER_PATH="$<TARGET_FILE:qindaqt-private-lock-pam>"
        QINDAQT_PRIVATE_PAM_MODULE_PATH="$<TARGET_FILE:qindaqt-private-pam-witness>")
    set(QINDAQT_PRIVATE_REEF_QML_ROOT "" CACHE PATH "Private built Reef prerequisite QML root")
    if(QINDAQT_PRIVATE_REEF_QML_ROOT)
        if(NOT EXISTS "${QINDAQT_PRIVATE_REEF_QML_ROOT}/QindaQt/CircuitReef/qmldir")
            message(FATAL_ERROR "Private Reef QML prerequisite is unavailable")
        endif()
        target_compile_definitions(qindaqt-private-lock-greeter PRIVATE
            QINDAQT_PRIVATE_REEF_QML_ROOT="${QINDAQT_PRIVATE_REEF_QML_ROOT}")
    endif()
    add_dependencies(qindaqt-private-lock-greeter qindaqt-private-lock-pam
        qindaqt-private-pam-witness qindaqt_session_lock_integration)
endif()
