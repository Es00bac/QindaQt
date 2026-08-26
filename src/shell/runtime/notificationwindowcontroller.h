// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QMetaObject>
#include <QPointer>
#include <QVariantMap>

#include <memory>

class QQmlComponent;
class QQmlEngine;
class QQuickWindow;
class QScreen;

namespace QindaQt::Services::NotificationPresentationModel {
class NotificationPresentationController;
}

namespace QindaQt::Shell {

class NotificationWindowController final {
public:
    NotificationWindowController(
        QQmlEngine &engine,
        Services::NotificationPresentationModel::NotificationPresentationController &
            presentation,
        QVariantMap theme);
    ~NotificationWindowController();

    [[nodiscard]] bool reconcile(QScreen *screen, QString *error = nullptr);
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
