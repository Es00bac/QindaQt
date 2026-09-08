# SPDX-License-Identifier: GPL-3.0-or-later

# This row is intentionally separate from fixture-only desktop-controls tests:
# it starts the installed session, KGlobalAccel, Audio1/PipeWire, notification
# host and Spectacle in one private namespace before injecting hardware keys.
if(TARGET qindaqt-desktop-controls
   AND TARGET qindaqt-shell
   AND TARGET qindaqt-session
   AND TARGET qindaqt-notification-host
   AND TARGET qindaqt-settings-service
   AND TARGET qindaqt-audio-service
   AND TARGET qindaqt_compositor
   AND TARGET qindaqt_decoration
   AND TARGET KF6::GlobalAccel
   AND QINDAQT_DESKTOP_BWRAP
   AND QINDAQT_DESKTOP_DBUS_DAEMON
   AND QINDAQT_KWIN_WAYLAND
   AND QINDAQT_PIPEWIRE
   AND QINDAQT_WIREPLUMBER
   AND QINDAQT_PW_CLI
   AND QINDAQT_WPCTL
   AND QINDAQT_KBUILDSYCOCA
   AND QINDAQT_SPECTACLE)
    qt_add_executable(
        qindaqt-daily-controls-live-probe
        compositorprobeclient.cpp
        compositorprobeclient.h
        hybridtestinputdriver.cpp
        hybridtestinputdriver.h
        dailycontrolsprobe.cpp
    )
    target_link_libraries(
        qindaqt-daily-controls-live-probe
        PRIVATE QindaQt::AudioClient KF6::GlobalAccel Qt6::Core Qt6::DBus Qt6::Gui
    )
    set_target_properties(qindaqt-daily-controls-live-probe PROPERTIES CXX_EXTENSIONS OFF)
    qindaqt_enable_warnings(qindaqt-daily-controls-live-probe)
    add_dependencies(
        qindaqt-daily-controls-live-probe
        qindaqt-desktop-controls qindaqt-shell qindaqt-session
        qindaqt-notification-host qindaqt-settings-service qindaqt-audio-service
        qindaqt_compositor
    )

    add_test(
        NAME desktop.daily-controls.live
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/test_daily_controls_live.py"
            --build-root "${CMAKE_BINARY_DIR}"
            --source-root "${PROJECT_SOURCE_DIR}"
            --cmake "${CMAKE_COMMAND}"
            --bwrap "${QINDAQT_DESKTOP_BWRAP}"
            --python "${Python3_EXECUTABLE}"
            --dbus-daemon "${QINDAQT_DESKTOP_DBUS_DAEMON}"
            --kwin-wayland "${QINDAQT_KWIN_WAYLAND}"
            --pipewire "${QINDAQT_PIPEWIRE}"
            --wireplumber "${QINDAQT_WIREPLUMBER}"
            --pw-cli "${QINDAQT_PW_CLI}"
            --wpctl "${QINDAQT_WPCTL}"
            --kbuildsycoca "${QINDAQT_KBUILDSYCOCA}"
            --spectacle "${QINDAQT_SPECTACLE}"
            --pipewire-config "${QINDAQT_PIPEWIRE_TEST_CONFIG}"
            --probe "$<TARGET_FILE:qindaqt-daily-controls-live-probe>"
            --bin-directory "${KDE_INSTALL_BINDIR}"
            --plugin-relative "${KDE_INSTALL_PLUGINDIR}/kwin/plugins/$<TARGET_FILE_NAME:qindaqt_compositor>"
            --decoration-relative "${KDE_INSTALL_PLUGINDIR}/${KDECORATION_PLUGIN_DIR}/$<TARGET_FILE_NAME:qindaqt_decoration>"
            --settings-service-directory "${CMAKE_INSTALL_DATADIR}/dbus-1/services"
            --audio-service-directory "${KDE_INSTALL_DBUSSERVICEDIR}"
    )
    set_tests_properties(
        desktop.daily-controls.live
        PROPERTIES
            TIMEOUT 180
            RUN_SERIAL TRUE
            RESOURCE_LOCK qindaqt-private-session
            SKIP_RETURN_CODE 77
            LABELS "integration;install;session;desktop-controls;audio;notification;screenshot;input"
    )
endif()
