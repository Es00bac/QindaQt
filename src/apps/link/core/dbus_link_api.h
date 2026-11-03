// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "link_api.h"

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusServiceWatcher>

#include <memory>

namespace QindaQt::LinkApp {

// LinkApi over `org.qindaqt.Link1` on the injected bus. Calls auto-start the service through its
// activation file; availability follows the well-known name.
// AGENT-CONTRACT: the method and signal names here are docs/wiki/reference/link1-v1.md; change
// both together.
class DbusLinkApi final : public LinkApi {
    Q_OBJECT
public:
    explicit DbusLinkApi(QDBusConnection bus, QObject *parent = nullptr);
    [[nodiscard]] bool available() const override { return m_available; }

    void devices(Reply done) override;
    void pendingPairings(Reply done) override;
    void confirmPairing(const QString &id, bool accept) override;
    void unpair(const QString &id) override;
    void listThreads(const QString &device, int limit, qint64 before, qint64 beforeId, Reply done) override;
    void listMessages(const QString &device, qint64 thread, int limit, qint64 before, const QString &beforeKind,
                      qint64 beforeId, Reply done) override;
    void getPart(const QString &device, qint64 part, int maxEdge, Reply done) override;
    void send(const QString &device, const QStringList &addrs, const QString &text, Reply done) override;
    void markRead(const QString &device, qint64 thread, Reply done) override;
    void listApps(Reply done) override;
    void setAppMuted(const QString &pkg, bool muted) override;
    void currentNotifications(Reply done) override;
    void reply(const QString &device, const QString &key, const QString &action, const QString &text,
               Reply done) override;
    void adbDevices(Reply done) override;
    void setupPhone(const QString &serial, Reply done) override;
    void setupStatus(const QString &serial, Reply done) override;

private:
    void call(const QString &interface, const QString &path, const QString &method, const QVariantList &args,
              Reply done, int timeoutMs = 15000);
    void connectSignals();
    void setAvailable(bool available);

    QDBusConnection m_bus;
    std::unique_ptr<QDBusServiceWatcher> m_watcher;
    bool m_available = false;
};

} // namespace QindaQt::LinkApp
