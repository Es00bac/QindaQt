# SPDX-License-Identifier: GPL-3.0-or-later
find_package(QindaTK CONFIG REQUIRED)
qt_add_executable(qindaqt-keyring-prompt main.cpp prompt_controller.cpp prompt_controller.h)
qt_add_qml_module(qindaqt-keyring-prompt URI QindaQt.KeyringPrompt VERSION 1.0
    RESOURCE_PREFIX /qt/qml QML_FILES Main.qml)
target_link_libraries(qindaqt-keyring-prompt PRIVATE qindaqt_keyring_service
    QindaQt::AuthenticationOverlay QindaTK::qindatk Qt6::Qml Qt6::Quick Qt6::QuickControls2)
qindaqt_enable_warnings(qindaqt-keyring-prompt)
install(TARGETS qindaqt-keyring-prompt RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT QindaQtKeyring)
