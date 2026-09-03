// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/power_service/power_collaborators.h>

#include <QtCore/QObject>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusServiceWatcher>

namespace QindaQt::Power::Upstream {

// AGENT-CONTRACT: Production session-authority adapter for
// org.freedesktop.login1.Manager on an injected bus connection. It observes
// only lid/dock/sleep truth and the sanitized four-field inhibitor summary;
// UID and PID from upstream ListInhibitors are dropped structurally. logind
// exposes LidClosed but no lid-present property, so lidPresent is derived
// conservatively: it becomes true only after a closed lid has been observed
// within the current run and never claims a lid that was not proven. Sleep
// truth follows the PrepareForSleep(b) signal (logind does not emit
// PropertiesChanged for it), and inhibitors are re-read on resume because
// inhibitor sets change across sleep.
class LogindSessionCollaborator final : public SessionCollaborator
{
    Q_OBJECT

public:
    explicit LogindSessionCollaborator(const QDBusConnection &upstreamConnection,
                                       QObject *parent = nullptr);
    ~LogindSessionCollaborator() override;

    quint64 start() override;
    void stop() override;

private:
    void scheduleUnavailable(quint64 generation, const QString &reasonCode);
    void refreshSession(quint64 generation);
    void readProperties(quint64 generation);
    void readInhibitors(quint64 generation);
    void publishFacts(quint64 generation);
    void onLogindOwnerChanged(const QString &name, const QString &oldOwner,
                              const QString &newOwner);
    [[nodiscard]] bool runningGeneration(quint64 generation) const;

    QDBusConnection m_connection;
    QDBusServiceWatcher *m_watcher = nullptr;
    QList<Inhibitor> m_inhibitors;
    bool m_lidClosed = false;
    bool m_docked = false;
    bool m_preparingForSleep = false;
    bool m_lidProven = false;
    int m_pendingReads = 0;
    quint64 m_generation = 0;
    quint64 m_nextGeneration = 0;
    bool m_running = false;

private Q_SLOTS:
    void onLogindPropertiesChanged(const class QDBusMessage &message);
    void onPrepareForSleep(bool start);
};

} // namespace QindaQt::Power::Upstream
