// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "transport.h"
#include <QMap>
#include <functional>
#include <QSet>
namespace QindaQt::Services::Shortcuts {
// Nonblocking GUI-thread lease for resident input integrations. The native
// Shortcuts1 authority retains conflict/assignment truth. Register preserves
// existing native assignments by reading them before reclaiming; only explicitly
// edited IDs receive SetShortcuts. Teardown deactivates rather than deleting
// saved assignments (the existing compatibility release wire; ADR-0427).
class ShortcutLease final : public QObject {
    Q_OBJECT
public:
    ShortcutLease(QDBusConnection bus, QString component, QObject *parent = nullptr);
    ~ShortcutLease() override;
    void replace(QList<Binding> desired, QSet<QString> assign = {});
Q_SIGNALS:
    void activated(const QString &action, quint64 timestamp);
    void status(const QString &action, const QString &reason, const QStringList &actual);
private Q_SLOTS:
    void activatedOnBus(const QString &component, const QString &action, quint64 timestamp);
    void bindingsChanged();
private:
    void resolveOwner();
    void deactivate(const QString &action);
    void attach(const QString &owner);
    void reconcile();
    void next();
    void observe();
    void request(const QString &method, const QVariantList &args, std::function<void(const QDBusMessage &)> done);
    QDBusConnection m_bus;
    QDBusServiceWatcher m_watch;
    QString m_component, m_owner;
    QList<Binding> m_desired, m_queue;
    QStringList m_remove;
    QSet<QString> m_registered, m_assign, m_ready;
    QMap<QString, QString> m_errors;
    QMap<QString, QStringList> m_existing;
    quint64 m_generation = 0;
    bool m_configured = false, m_busy = false, m_observing = false, m_observeAgain = false;
};
} // namespace QindaQt::Services::Shortcuts
