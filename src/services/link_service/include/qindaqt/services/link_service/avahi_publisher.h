// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_service/service_publisher.h>

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusServiceWatcher>

namespace QindaQt::Link {

// Publishes through org.freedesktop.Avahi (system bus) with one EntryGroup.
//
// AGENT-NOTE: Avahi keeps groups only for the lifetime of the client that
// created them, so a daemon restart (name owner change) re-creates the group
// from the remembered advert. A collision (group state 3) asks Avahi for an
// alternative name and re-adds the service under it.
// Ownership: borrows nothing but the bus connection. Threading: one thread.
class AvahiPublisher final : public ServicePublisher {
    Q_OBJECT
public:
    explicit AvahiPublisher(QDBusConnection bus, QObject *parent = nullptr);
    ~AvahiPublisher() override;
    void publish(const ServiceAdvert &advert) override;
    void withdraw() override;
    [[nodiscard]] QString state() const override { return m_state; }

private Q_SLOTS:
    void onGroupState(int state, const QString &error);

private:
    void attempt();
    void addAndCommit();
    void freeGroup();
    void setState(const QString &state);

    QDBusConnection m_bus;
    QDBusServiceWatcher m_watcher;
    ServiceAdvert m_advert;
    QString m_activeName;
    QString m_groupPath;
    QString m_state = QStringLiteral("off");
    bool m_wanted = false;
    quint64 m_generation = 0;   // discards replies belonging to a withdrawn attempt
};

} // namespace QindaQt::Link
