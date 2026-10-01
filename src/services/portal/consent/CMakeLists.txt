# SPDX-License-Identifier: GPL-3.0-or-later
find_package(QindaTK CONFIG REQUIRED)
qt_add_executable(qindaqt-portal-consent main.cpp consent_controller.cpp consent_controller.h)
qt_add_qml_module(qindaqt-portal-consent URI QindaQt.PortalConsent VERSION 1.0
    RESOURCE_PREFIX /qt/qml QML_FILES Main.qml)
target_link_libraries(qindaqt-portal-consent PRIVATE QindaQt::PortalFoundation QindaQt::ForeignParent
    QindaTK::qindatk Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2)
qindaqt_enable_warnings(qindaqt-portal-consent)
install(TARGETS qindaqt-portal-consent RUNTIME DESTINATION "${KDE_INSTALL_LIBEXECDIR}" COMPONENT QindaQtPortalP0)
