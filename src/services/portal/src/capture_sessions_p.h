// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/portal/request_registry.h>
#include <QHash>
#include <QStringList>
#include <QTimer>
#include <QDBusVirtualObject>
#include <memory>
namespace QindaQt::Services::Portal {
enum class CaptureSessionPhase { Created, Selected, Starting, Streaming };
// Portal-private Session wire/lifetime, never capture/presentation policy.
class CaptureSessions final : public QObject {
    Q_OBJECT
public:
    struct Entry { QString frontend, caller, app; int pidfd = -1; CaptureSessionPhase phase = CaptureSessionPhase::Created; RequestToken pending = 0; QDBusVirtualObject *object = nullptr; bool multiple = false; quint32 cursorMode = 1; quint32 persistMode = 0; QStringList restore{}; };
    CaptureSessions(QDBusConnection, RequestRegistry &, QObject *parent = nullptr);
    ~CaptureSessions() override;
    bool create(const QDBusMessage &, const QString &requestPath, const QString &sessionPath, const QString &app);
    bool authenticated(const QDBusMessage &, const QString &, const QString &) const;
    bool requestMatches(const QString &session, const QString &request) const;
    bool live(const QString &) const;
    Entry *entry(const QString &); // caller must check current admission before use
    void close(const QString &, bool notify = true);
    void clear();
Q_SIGNALS:
    void retired(const QString &);
private:
    QDBusConnection m_bus; RequestRegistry &m_requests; QHash<QString, Entry> m_entries; QTimer m_lifetime;
};
}
