# SPDX-License-Identifier: GPL-3.0-or-later
find_package(QindaTK CONFIG REQUIRED)
find_package(Qt6 6.11 REQUIRED COMPONENTS Quick Qml QuickControls2 GuiPrivate Test)
if(NOT TARGET qindaqt_osk_core)
    message(FATAL_ERROR "Native locker requires the first-party pure OSK model")
endif()
set(_lock_greeter_sources authentication_controller.cpp authentication_controller.h authentication_ports.h
    greeter_log.cpp greeter_log.h input_activity_filter.cpp input_activity_filter.h
    native_authentication_ports.cpp native_authentication_ports.h
    output_window_supervisor.cpp output_window_supervisor.h retry_backoff.h
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

if(BUILD_TESTING)
    # Pure behavior tests: no Wayland, no QML, no PAM child. Each links only the
    # sources of the behavior it covers (module boundary: the controller depends
    # on the ports in authentication_ports.h, never on the real collaborators).
    function(qindaqt_lock_greeter_unit_test name source)
        qt_add_executable(${name} ${source} ${ARGN})
        target_include_directories(${name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
        target_link_libraries(${name} PRIVATE Qt6::Test)
        qindaqt_enable_warnings(${name})
    endfunction()
    qindaqt_lock_greeter_unit_test(tst_lock_greeter_controller tests/tst_authentication_controller.cpp
        authentication_controller.cpp authentication_controller.h authentication_ports.h
        greeter_log.cpp greeter_log.h)
    target_link_libraries(tst_lock_greeter_controller PRIVATE qindaqt_lock_authentication)
    add_test(NAME lock.greeter-controller COMMAND tst_lock_greeter_controller)
    qindaqt_lock_greeter_unit_test(tst_lock_greeter_input tests/tst_input_activity.cpp
        input_activity_filter.cpp input_activity_filter.h)
    target_link_libraries(tst_lock_greeter_input PRIVATE Qt6::Gui)
    add_test(NAME lock.greeter-input COMMAND tst_lock_greeter_input)
    qindaqt_lock_greeter_unit_test(tst_lock_greeter_outputs tests/tst_output_window_supervisor.cpp
        output_window_supervisor.cpp output_window_supervisor.h retry_backoff.h)
    add_test(NAME lock.greeter-outputs COMMAND tst_lock_greeter_outputs)
    # The real Main.qml + controller + input filter on an offscreen window.
    qt_add_executable(tst_lock_greeter_ui tests/tst_lock_greeter_ui.cpp
        authentication_controller.cpp authentication_controller.h authentication_ports.h
        greeter_log.cpp greeter_log.h input_activity_filter.cpp input_activity_filter.h
        touch_keyboard.cpp touch_keyboard.h saver_presentation.cpp saver_presentation.h)
    qindaqt_lock_greeter_common(tst_lock_greeter_ui)
    target_link_libraries(tst_lock_greeter_ui PRIVATE Qt6::Test qindaqt_lock_authentication)
    add_test(NAME lock.greeter-ui COMMAND tst_lock_greeter_ui)
    set_tests_properties(lock.greeter-ui PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1"
        TIMEOUT 120 LABELS "lock;greeter")
    # The real private greeter binary against a fake ext-session-lock compositor
    # (the owner's timeout scenario end to end). Needs the fake compositor library
    # from lock_protocol, which exists only when libwayland-server is available.
    if(TARGET qindaqt_lock_fake_compositor)
        qt_add_executable(tst_lock_greeter_process tests/tst_lock_greeter_process.cpp)
        target_link_libraries(tst_lock_greeter_process PRIVATE qindaqt_lock_fake_compositor Qt6::Test)
        target_compile_definitions(tst_lock_greeter_process PRIVATE
            QINDAQT_PRIVATE_GREETER_PATH="$<TARGET_FILE:qindaqt-private-lock-greeter>")
        add_dependencies(tst_lock_greeter_process qindaqt-private-lock-greeter)
        qindaqt_enable_warnings(tst_lock_greeter_process)
        add_test(NAME lock.greeter-process COMMAND tst_lock_greeter_process)
        set_tests_properties(lock.greeter-process PROPERTIES TIMEOUT 120 LABELS "lock;greeter")
    endif()
    set_tests_properties(lock.greeter-input lock.greeter-outputs PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FATAL_WARNINGS=1" TIMEOUT 60 LABELS "lock;greeter")
    # Expects the greeter's exit-reason warnings (QTest::ignoreMessage) and fails
    # on any other warning (QTest::failOnWarning), so fatal warnings stay off.
    set_tests_properties(lock.greeter-controller PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FATAL_WARNINGS=0" TIMEOUT 60 LABELS "lock;greeter")
endif()
