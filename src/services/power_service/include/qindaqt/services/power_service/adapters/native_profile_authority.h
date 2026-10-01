// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QtCore/QObject>
#include <QtDBus/QDBusConnection>
#include <memory>
class QDBusServiceWatcher;
namespace QindaQt::Power::Upstream {
// Watches the constructing session bus only. Explicit exclusive cutover and
// confirmed absence of the legacy writer admit native source holds. A bus
// error or legacy arrival revokes admission; no activation or retirement.
class NativeProfileAuthority final : public QObject {
    Q_OBJECT
public:
    NativeProfileAuthority(QDBusConnection connection, bool exclusive,
                           QObject *parent = nullptr);
    ~NativeProfileAuthority() override;
    void start();
    [[nodiscard]] bool admitted() const noexcept { return m_admitted; }
Q_SIGNALS:
    void admissionChanged(bool admitted);
private Q_SLOTS:
    void resolve();
    void disconnected();
private:
    QDBusConnection m_connection;
    std::unique_ptr<QDBusServiceWatcher> m_watcher;
    bool m_exclusive;
    bool m_admitted = false;
};
}
