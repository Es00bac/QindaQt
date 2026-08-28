// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QMetaObject>
#include <QPointer>
#include <QJsonObject>
#include <QVariantMap>

#include <memory>

class QQmlComponent;
class QQmlEngine;
class QQuickWindow;
class QScreen;

namespace QindaQt::Services::NotificationPresentationModel {
class NotificationPresentationController;
}
namespace QindaQt::Services::SettingsClient {
class DoNotDisturbController;
}

namespace QindaQt::Shell {

class SettingsRouteLauncher;

class NotificationWindowController final {
public:
    NotificationWindowController(
        QQmlEngine &engine,
        Services::NotificationPresentationModel::NotificationPresentationController &
            presentation,
        Services::SettingsClient::DoNotDisturbController &quietingSettings,
        SettingsRouteLauncher &settingsLauncher,
        QVariantMap theme);
    ~NotificationWindowController();

    [[nodiscard]] bool reconcile(QScreen *screen, QString *error = nullptr);
    // Development evidence is a read-only projection of the two production
    // windows. It never creates, focuses, maps, or mutates a surface.
    [[nodiscard]] QJsonObject evidence() const;
    void reset() noexcept;

private:
    [[nodiscard]] bool ensureComponents(QString *error);
    [[nodiscard]] std::unique_ptr<QQuickWindow> createWindow(
        QQmlComponent &component, const QString &role, QString *error);
    [[nodiscard]] bool createWindows(QScreen &screen, QString *error);
    [[nodiscard]] bool resizeWindows(QScreen &screen, int popupCount,
                                     QString *error);
    void updateVisibility();

    QQmlEngine &m_engine;
    Services::NotificationPresentationModel::NotificationPresentationController &
        m_presentation;
    Services::SettingsClient::DoNotDisturbController &m_quietingSettings;
    SettingsRouteLauncher &m_settingsLauncher;
    QVariantMap m_theme;
    std::unique_ptr<QQmlComponent> m_popupComponent;
    std::unique_ptr<QQmlComponent> m_centerComponent;
    std::unique_ptr<QQuickWindow> m_popupWindow;
    std::unique_ptr<QQuickWindow> m_centerWindow;
    QPointer<QScreen> m_screen;
    QMetaObject::Connection m_popupCountConnection;
    QMetaObject::Connection m_centerOpenConnection;
    QMetaObject::Connection m_operationBusyConnection;
    QMetaObject::Connection m_operationErrorConnection;
};

} // namespace QindaQt::Shell
