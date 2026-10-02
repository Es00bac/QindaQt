// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/portal/request_registry.h>
#include <QDBusServiceWatcher>
#include <QDBusVirtualObject>
#include <QHash>

class QSocketNotifier;
namespace QindaQt::Services::Portal::RemoteInput {
enum class RemotePhase { Created, Selected, Starting, Started };
// Module-private standard Session objects for RemoteDesktop. Holds wire and
// actor lifetime only; consent and EIS policy stay in the adaptor.
// AGENT-GUARD: a session belongs to the frontend owner that created it, the
// caller encoded in its standard handle (checked live by unique name and
// PIDFD) and its app ID. Losing any of them closes the session, which the
// adaptor turns into consent cancellation and compositor EIS disconnect.
class RemoteSessions final : public QObject {
    Q_OBJECT
public:
    struct Entry {
        QString frontend, caller, app;
        int pidfd = -1;
        RemotePhase phase = RemotePhase::Created;
        quint32 devices = 0;
        RequestToken pending = 0;
        quint64 eisTicket = 0;
        QDBusMessage eisCall;
        QString compositor;
        int cookie = 0;
        QDBusVirtualObject *object = nullptr;
        QSocketNotifier *exit = nullptr;
    };
    RemoteSessions(QDBusConnection, RequestRegistry &, QObject *parent = nullptr);
    ~RemoteSessions() override;
    bool create(const QDBusMessage &, const QString &request, const QString &session, const QString &app);
    bool authenticated(const QDBusMessage &, const QString &session, const QString &app) const;
    bool requestMatches(const QString &session, const QString &request) const;
    bool live(const QString &session) const;
    Entry *entry(const QString &session); // revalidate admission before use
    QString sessionForTicket(quint64 ticket) const;
    void close(const QString &session, bool notify = true);
    void clear();
Q_SIGNALS:
    // Emitted after the object is unregistered; the entry is a final snapshot.
    void retired(const QString &session, const RemoteSessions::Entry &entry);
private:
    void sweep();
    QDBusConnection m_bus;
    RequestRegistry &m_requests;
    QDBusServiceWatcher m_watcher;
    QHash<QString, Entry> m_entries;
};
} // namespace QindaQt::Services::Portal::RemoteInput
