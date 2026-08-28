# SPDX-License-Identifier: GPL-3.0-or-later

# Keep the staged production notification matrix cohesive without growing the
# already broad session registry into a source-shape exception.
# Imported targets are directory-scoped unless promoted. The shell finds this
# package in src/shell, so tests/session must import it in its own scope before
# using KF6::GlobalAccel or the entire live target would be silently omitted.
if(TARGET qindaqt-shell AND NOT TARGET KF6::GlobalAccel)
    find_package(KF6GlobalAccel 6.0 REQUIRED CONFIG)
endif()

if(TARGET qindaqt-shell
   AND TARGET qindaqt-session
   AND TARGET qindaqt-notification-host
   AND TARGET qindaqt-settings-service
   AND TARGET qindaqt-settings
   AND TARGET KF6::GlobalAccel
   AND QINDAQT_BUSCTL
   AND NOT IS_ABSOLUTE "${KDE_INSTALL_BINDIR}"
   AND NOT IS_ABSOLUTE "${KDE_INSTALL_PLUGINDIR}")
    qt_add_executable(
        qindaqt-notification-live-probe
        compositorprobeclient.cpp
        compositorprobeclient.h
        hybridtestinputdriver.cpp
        hybridtestinputdriver.h
        notificationliveevidenceclient.cpp
        notificationliveevidenceclient.h
        notificationlivekeyboard.cpp
        notificationlivekeyboard.h
        notificationlivelock.cpp
        notificationlivelock.h
        notificationliveprobe.cpp
        notificationliveresident.cpp
        notificationliveresident.h
        notificationliveruntime.cpp
        notificationliveruntime.h
        notificationlivesettingsphases.cpp
        notificationlivesettingsphases.h
        notificationliveshortcut.cpp
        notificationliveshortcut.h
        notificationlivesurfaces.cpp
        notificationlivesurfaces.h
        notificationliveworkflow.cpp
        notificationliveworkflow.h
    )
    target_link_libraries(
        qindaqt-notification-live-probe
        PRIVATE KF6::GlobalAccel Qt6::Core Qt6::DBus Qt6::Gui
    )
    set_target_properties(
        qindaqt-notification-live-probe PROPERTIES CXX_EXTENSIONS OFF
    )
    qindaqt_enable_warnings(qindaqt-notification-live-probe)
    add_dependencies(
        qindaqt-notification-live-probe
        qindaqt-shell
        qindaqt-session
        qindaqt-notification-host
        qindaqt-settings-service
        qindaqt-settings
        qindaqt_compositor
    )

    function(qindaqt_add_notification_live_test test_name scenario_name)
        set(options RACE_TEN)
        cmake_parse_arguments(ARG "${options}" "" "" ${ARGN})
        set(_repeat 1)
        set(_timeout 240)
        if(ARG_RACE_TEN)
            set(_repeat 10)
            # AGENT-GUARD: The outer timeout must exceed ten 180-second inner
            # budgets so CTest cannot preempt private process-group teardown.
            set(_timeout 2400)
        endif()
        add_test(
            NAME "shell.notification-live.${test_name}"
            COMMAND
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/test_notification_live_nested.py"
                --cmake "${CMAKE_COMMAND}"
                --build-directory "${CMAKE_BINARY_DIR}"
                --install-prefix
                    "${CMAKE_CURRENT_BINARY_DIR}/notification-live-stage-${test_name}"
                --dbus-runner "${QINDAQT_DBUS_RUN_SESSION}"
                --busctl "${QINDAQT_BUSCTL}"
                --probe "$<TARGET_FILE:qindaqt-notification-live-probe>"
                --scenario
                    "${PROJECT_SOURCE_DIR}/tests/scenarios/${scenario_name}.json"
                --configuration "$<CONFIG>"
                --repeat "${_repeat}"
                --launcher-relative
                    "${KDE_INSTALL_BINDIR}/qindaqt-wm${CMAKE_EXECUTABLE_SUFFIX}"
                --session-relative
                    "${KDE_INSTALL_BINDIR}/qindaqt-session${CMAKE_EXECUTABLE_SUFFIX}"
                --notification-host-relative
                    "${KDE_INSTALL_BINDIR}/qindaqt-notification-host${CMAKE_EXECUTABLE_SUFFIX}"
                --settings-service-relative
                    "${KDE_INSTALL_BINDIR}/qindaqt-settings-service${CMAKE_EXECUTABLE_SUFFIX}"
                --settings-app-relative
                    "${KDE_INSTALL_BINDIR}/qindaqt-settings${CMAKE_EXECUTABLE_SUFFIX}"
                --shell-relative
                    "${KDE_INSTALL_BINDIR}/qindaqt-shell${CMAKE_EXECUTABLE_SUFFIX}"
                --compositor-plugin-relative
                    "${KDE_INSTALL_PLUGINDIR}/kwin/plugins/$<TARGET_FILE_NAME:qindaqt_compositor>"
        )
        set_tests_properties(
            "shell.notification-live.${test_name}"
            PROPERTIES
                TIMEOUT "${_timeout}"
                RUN_SERIAL TRUE
                LABELS
                    "integration;shell;notification;wayland;security;installed"
        )
    endfunction()

    qindaqt_add_notification_live_test(1080p single-1080p)
    qindaqt_add_notification_live_test(wuxga single-wuxga)
    qindaqt_add_notification_live_test(1440p single-1440p)
    qindaqt_add_notification_live_test(scale-125 single-1080p-125)
    qindaqt_add_notification_live_test(scale-150 single-1080p-150)
    qindaqt_add_notification_live_test(race-10x single-1080p RACE_TEN)
endif()
