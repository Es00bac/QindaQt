# SPDX-License-Identifier: GPL-3.0-or-later
#
# qindaqt-link: the QindaLink companion app (docs/wiki/apps/qindalink.md). QML on QindaTK; the view
# models are a static library so tests link them without an engine or a window.
find_package(Qt6 6.11 REQUIRED COMPONENTS DBus Gui Qml Quick)

qt_add_library(qindaqt_link_app_view STATIC
    core/link_api.h
    core/dbus_link_api.cpp core/dbus_link_api.h
    core/fake_link_api.cpp core/fake_link_api.h
    core/json_list_model.cpp core/json_list_model.h
    core/message_json.cpp core/message_json.h
    core/app_preferences.cpp core/app_preferences.h
    core/process_runner.cpp core/process_runner.h
    core/app_options.cpp core/app_options.h
    ui/devices_view_model.cpp ui/devices_view_model.h
    ui/threads_view_model.cpp ui/threads_view_model.h
    ui/conversation_view_model.cpp ui/conversation_view_model.h
    ui/phone_apps_view_model.cpp ui/phone_apps_view_model.h
    ui/notifications_view_model.cpp ui/notifications_view_model.h
    ui/reply_view_model.cpp ui/reply_view_model.h
    ui/launcher.h
    app_shell/link_actions.cpp app_shell/link_actions.h
)
add_library(QindaQt::LinkAppView ALIAS qindaqt_link_app_view)
target_include_directories(qindaqt_link_app_view PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/core" "${CMAKE_CURRENT_SOURCE_DIR}/ui" "${CMAKE_CURRENT_SOURCE_DIR}/app_shell")
target_link_libraries(qindaqt_link_app_view PUBLIC Qt6::Core Qt6::DBus Qt6::Gui QindaQt::LinkClient QindaQt::AppShell)
target_compile_features(qindaqt_link_app_view PUBLIC cxx_std_20)
set_target_properties(qindaqt_link_app_view PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON AUTOUIC OFF)
qindaqt_enable_warnings(qindaqt_link_app_view)

qt_add_executable(qindaqt-link main.cpp)

# AGENT-CONTRACT: the interface is QML built on QindaTK (dev-libs/qindatk) through the module on Qt's
# QML import path, exactly like the System Monitor; nothing links QindaTK's C++ API.
qt_add_qml_module(qindaqt-link
    URI QindaQt.Link
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/ReplyWindow.qml
        qml/pages/DevicesPage.qml
        qml/pages/MessagesPage.qml
        qml/pages/PhoneAppsPage.qml
        qml/pages/NotificationsPage.qml
        qml/pages/InputPage.qml
        qml/pages/VaultPage.qml
        qml/parts/LinkMenuBar.qml
        qml/parts/LinkPanel.qml
        qml/parts/PairingCard.qml
        qml/parts/SetupSection.qml
        qml/parts/ThreadList.qml
        qml/parts/ConversationView.qml
        qml/parts/MessageBubble.qml
        qml/parts/ComposeBar.qml
        qml/parts/ServiceNotice.qml
)

target_link_libraries(qindaqt-link PRIVATE QindaQt::LinkAppView QindaQt::AppShellMenuExport Qt6::DBus Qt6::Gui Qt6::Quick)
target_compile_features(qindaqt-link PRIVATE cxx_std_20)
set_target_properties(qindaqt-link PROPERTIES CXX_EXTENSIONS OFF AUTOUIC OFF)
qindaqt_enable_warnings(qindaqt-link)

# AGENT-GUARD: AppShell's backing library is installed into Qt's QML tree, not the system library
# path, so a bare install gives an executable that cannot start: "libqindaqt_app_shell.so: cannot
# open shared object file". Same relative lookup as the System Monitor.
set(link_app_bindir "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}")
set(link_app_qmldir "${QT6_INSTALL_QML}")
cmake_path(ABSOLUTE_PATH link_app_qmldir BASE_DIRECTORY "${CMAKE_INSTALL_PREFIX}" NORMALIZE)
cmake_path(RELATIVE_PATH link_app_qmldir BASE_DIRECTORY "${link_app_bindir}" OUTPUT_VARIABLE link_app_qml_relative)
set_target_properties(qindaqt-link PROPERTIES
    INSTALL_RPATH "$ORIGIN/${link_app_qml_relative}/QindaQt/AppShell")

install(TARGETS qindaqt-link RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT QindaLinkApp)
install(FILES org.qindaqt.Link.desktop
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications" COMPONENT QindaLinkApp)
install(FILES icons/org.qindaqt.Link.svg
    DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps" COMPONENT QindaLinkApp)
