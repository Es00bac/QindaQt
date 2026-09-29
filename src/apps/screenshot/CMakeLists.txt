# SPDX-License-Identifier: GPL-3.0-or-later

# QindaQt Screenshot (ADR-0289): the native capture and OBS record tool that
# replaces Spectacle. QindaTK presentation over KWin's ScreenShot2, the
# desktop's notification service, ext-data-control clipboard publishing and
# the one obs-websocket client.
find_package(QindaTK CONFIG REQUIRED)

# Policy, platform ports and presentation models: everything a row can drive
# without the QML scene or a real compositor.
qt_add_library(qindaqt_screenshot_support STATIC
    capture/capture_file_policy.cpp capture/capture_file_policy.h
    capture/capture_request.cpp capture/capture_request.h
    capture/command_line.cpp capture/command_line.h
    capture/raw_capture_decoder.cpp capture/raw_capture_decoder.h
    capture/region_geometry.cpp capture/region_geometry.h
    platform/capture_port.h
    platform/clipboard_publisher.cpp platform/clipboard_publisher.h
    platform/desktop_actions.cpp platform/desktop_actions.h
    platform/kwin_capture_port.cpp platform/kwin_capture_port.h
    platform/result_notifier.cpp platform/result_notifier.h
    record/record_connection.cpp record/record_connection.h
    record/record_controller.cpp record/record_controller.h
    ui/capture_flow.cpp ui/capture_flow.h
    ui/capture_result.cpp ui/capture_result.h
    ui/frame_store.cpp ui/frame_store.h)
target_include_directories(qindaqt_screenshot_support PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/capture" "${CMAKE_CURRENT_SOURCE_DIR}/platform"
    "${CMAKE_CURRENT_SOURCE_DIR}/record" "${CMAKE_CURRENT_SOURCE_DIR}/ui")
# AGENT-CONTRACT: OBS goes through the shared client and the applet's pure
# presentation (its unavailable sentences), never a second websocket client
# (ADR-0201); the clipboard goes through the clipboard service's own
# data-control adapter (ADR-0289).
target_link_libraries(qindaqt_screenshot_support PUBLIC
    QindaQt::ScreenshotPreferences QindaQt::StreamingPreferences QindaQt::ObsClient
    QindaQt::ShellObsApplet QindaQt::ClipboardWaylandAdapter
    Qt6::Core Qt6::Gui Qt6::DBus Qt6::Quick)
target_compile_features(qindaqt_screenshot_support PUBLIC cxx_std_20)
set_target_properties(qindaqt_screenshot_support PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_screenshot_support)

qt_add_executable(qindaqt-screenshot main.cpp app/screenshot_app.cpp app/screenshot_app.h)
qt_add_qml_module(qindaqt-screenshot
    URI QindaQt.Screenshot VERSION 1.0 RESOURCE_PREFIX /qt/qml
    QML_FILES qml/Main.qml qml/CapturePage.qml qml/ResultPage.qml qml/RecordPage.qml
              qml/RegionOverlay.qml)
target_include_directories(qindaqt-screenshot PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(qindaqt-screenshot PRIVATE qindaqt_screenshot_support QindaQt::SettingsClient
    QindaTK::qindatk Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::DBus)
qindaqt_enable_warnings(qindaqt-screenshot)

# AGENT-CONTRACT: QindaTK's QindaQt token bridge loads backing libraries from
# the QML tree, outside the system library path; relocate like Voice does.
set(screenshot_bindir "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}")
set(screenshot_qmldir "${QT6_INSTALL_QML}")
cmake_path(ABSOLUTE_PATH screenshot_qmldir BASE_DIRECTORY "${CMAKE_INSTALL_PREFIX}" NORMALIZE)
cmake_path(RELATIVE_PATH screenshot_qmldir BASE_DIRECTORY "${screenshot_bindir}"
    OUTPUT_VARIABLE screenshot_qml_relative)
set_target_properties(qindaqt-screenshot PROPERTIES
    INSTALL_RPATH "$ORIGIN/${screenshot_qml_relative}/QindaQt/Tokens;$ORIGIN/${screenshot_qml_relative}/QindaQt/Controls")

# AGENT-CONTRACT (ADR-0289, as ADR-0241): KWin admits ScreenShot2 callers by
# matching their executable to an installed desktop entry, so Exec must be
# the exact installed path; configure it rather than hard-coding /usr/bin.
configure_file(org.qindaqt.Screenshot.desktop.in
    "${CMAKE_CURRENT_BINARY_DIR}/org.qindaqt.Screenshot.desktop" @ONLY)
install(TARGETS qindaqt-screenshot RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT Screenshot)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/org.qindaqt.Screenshot.desktop"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications" COMPONENT Screenshot)
install(FILES "${PROJECT_SOURCE_DIR}/data/icons/QindaQt/scalable/apps/org.qindaqt.Screenshot.svg"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps" COMPONENT Screenshot)
