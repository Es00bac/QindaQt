// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_service/pairing_presenter.h>

#include <QtCore/QHash>
#include <QtDBus/QDBusConnection>

namespace QindaQt::Link {

// org.freedesktop.Notifications presenter. The connection is injected so a
// private bus can be used; with no notification server the request simply is
// not shown (the Link1 PairingRequested signal still reaches the app).
class QtNotificationPresenter final : public PairingPresenter {
    Q_OBJECT
public:
    explicit QtNotificationPresenter(QDBusConnection bus, QObject *parent = nullptr);
    void present(const QString &deviceId, const QString &name, const QString &model, const QString &code, const QString &origin,
                 const QString &source) override;
    void dismiss(const QString &deviceId) override;

private Q_SLOTS:
    void onActionInvoked(uint id, const QString &key);
    void onClosed(uint id, uint reason);

private:

    QDBusConnection m_bus;
    QHash<uint, QString> m_byNotification;
    QHash<QString, uint> m_byDevice;
};

} // namespace QindaQt::Link
