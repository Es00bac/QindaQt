// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/power_service/power_collaborators.h>

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtCore/QSet>

#include <memory>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusServiceWatcher>

namespace QindaQt::Power::Upstream {

enum class SessionAction : quint32 {
    PowerOff = 0,
    Reboot = 1,
    Suspend = 2,
    Hibernate = 3,
};

struct AdmittedActions {
    bool powerOff = false;
    bool reboot = false;
    bool suspend = false;
    bool hibernate = false;
};

// AGENT-CONTRACT: logind session-action authority for the later PB-3 shell
// controller, on an injected bus connection. CanPowerOff/CanReboot/CanSuspend/
// CanHibernate define the admitted set: only a "yes" answer admits an action,
// because Power1 v1 provides no polkit UI ("challenge" stays unadmitted and
// the controller must surface it, not answer it). Actions are executed only
// through this admission gate with generation-fenced, exactly-once completion
// carrying CollaboratorOutcome values; an upstream owner replacement mid-flight
// completes as Uncertain. QindaQt never passes interactive=true.
class LogindActionAuthority final : public QObject
{
    Q_OBJECT

public:
    explicit LogindActionAuthority(const QDBusConnection &upstreamConnection,
                                   QObject *parent = nullptr);
    ~LogindActionAuthority() override;

    quint64 start();
    void stop();
    [[nodiscard]] const AdmittedActions &admittedActions() const noexcept;
    void refreshAdmittedActions();
    void submitAction(quint64 operationId, SessionAction action);

Q_SIGNALS:
    void admittedActionsChanged(const QindaQt::Power::Upstream::AdmittedActions &
                                actions);
    void actionFinished(quint64 generation, quint64 operationId,
                        const QindaQt::Power::CollaboratorOutcome &outcome);

private:
    struct PendingCanQuery {
        int outstanding = 0;
        quint64 serial = 0;
        QString owner;
        AdmittedActions actions;
    };

    void callCanAction(SessionAction action, const std::shared_ptr<PendingCanQuery> &query);
    void callExecuteAction(quint64 operationId, SessionAction action);
    void finishAction(quint64 generation, quint64 operationId,
                      CollaboratorStatus status, const QString &reasonCode);
    void onLogindOwnerChanged(const QString &name, const QString &oldOwner,
                              const QString &newOwner);
    [[nodiscard]] static QString canMethodName(SessionAction action);
    [[nodiscard]] static QString executeMethodName(SessionAction action);
    [[nodiscard]] bool runningGeneration(quint64 generation) const;

    QDBusConnection m_connection;
    QDBusServiceWatcher *m_watcher = nullptr;
    AdmittedActions m_admitted;
    QString m_activeOwner;
    QHash<quint64, QString> m_pendingActions;
    QSet<quint64> m_seenOperationIds;
    quint64 m_generation = 0;
    quint64 m_nextGeneration = 0;
    quint64 m_refreshSerial = 0;
    bool m_running = false;
};

} // namespace QindaQt::Power::Upstream

Q_DECLARE_METATYPE(QindaQt::Power::Upstream::AdmittedActions)
