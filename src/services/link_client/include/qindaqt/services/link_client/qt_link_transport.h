// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_client/link_transport.h>

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusServiceWatcher>

#include <memory>

namespace QindaQt::Link {

// QtDBus implementation of LinkTransport; the connection is injected.
class QtLinkTransport final : public LinkTransport {
    Q_OBJECT
public:
    explicit QtLinkTransport(QDBusConnection bus, QObject *parent = nullptr);
    ~QtLinkTransport() override;
    [[nodiscard]] bool start(QString *error = nullptr) override;
    void stop() override;
    void requestDevices(quint64 token) override;
    void confirmPairing(const QString &id, bool accept) override;
    void unpair(const QString &id) override;
    void ring(const QString &id) override;

private Q_SLOTS:
    void onOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner);
    void onDevicesChanged(const QString &json);
    void onPairingRequested(const QString &id, const QString &name, const QString &model, const QString &code);

private:
    void call(const QString &method, const QVariantList &args);

    QDBusConnection m_bus;
    std::unique_ptr<QDBusServiceWatcher> m_watcher;
    bool m_running = false;
    bool m_available = false;
    quint64 m_epoch = 0;  // bumps on every availability transition
};

} // namespace QindaQt::Link
