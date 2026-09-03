// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtCore/QtTypes>
#include <QtDBus/QDBusConnection>

#include <memory>
#include <optional>

class QDBusServiceWatcher;

namespace QindaQt::Services::SessionActions {

enum class SessionAction : quint8 {
    Lock,
    Logout,
    Suspend,
    Reboot,
    PowerOff,
};

enum class ActionStatus : quint8 {
    Succeeded,
    Rejected,
    Unavailable,
    Busy,
    Uncertain,
};

struct SessionActionAvailability final {
    bool lock = false;
    bool logout = false;
    bool suspend = false;
    bool reboot = false;
    bool powerOff = false;

    [[nodiscard]] bool operator==(const SessionActionAvailability &) const = default;
};

struct SessionActionResult final {
    quint64 requestId = 0;
    SessionAction action = SessionAction::Lock;
    ActionStatus status = ActionStatus::Unavailable;
    QString reasonCode;
};

// GUI-thread asynchronous client for the three platform authorities used by
// session controls. Both bus connections are injected and borrowed by value;
// tests may point them at one private broker. Owner changes trigger one
// coalesced refresh, never a recurring poll.
//
// AGENT-CONTRACT: cached capability results are presentation facts only. Lock
// repeats its interface probe and every Can* action repeats its admission query
// against the exact current unique owner. One request may be live and its
// bounded deadline never replays an uncertain mutation.
class SessionActionsClient final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool canLock READ canLock NOTIFY availabilityChanged)
    Q_PROPERTY(bool canLogout READ canLogout NOTIFY availabilityChanged)
    Q_PROPERTY(bool canSuspend READ canSuspend NOTIFY availabilityChanged)
    Q_PROPERTY(bool canReboot READ canReboot NOTIFY availabilityChanged)
    Q_PROPERTY(bool canPowerOff READ canPowerOff NOTIFY availabilityChanged)
    Q_PROPERTY(bool pending READ pending NOTIFY pendingChanged)
    Q_PROPERTY(QString feedback READ feedback NOTIFY feedbackChanged)

public:
    static constexpr int RefreshTimeoutMilliseconds = 3'000;
    static constexpr int ActionTimeoutMilliseconds = 5'000;

    SessionActionsClient(QDBusConnection sessionBus,
                         QDBusConnection systemBus,
                         QObject *parent = nullptr);
    ~SessionActionsClient() override;

    void start();
    void stop();
    void refresh();

    [[nodiscard]] const SessionActionAvailability &availability() const noexcept;
    [[nodiscard]] bool canLock() const noexcept;
    [[nodiscard]] bool canLogout() const noexcept;
    [[nodiscard]] bool canSuspend() const noexcept;
    [[nodiscard]] bool canReboot() const noexcept;
    [[nodiscard]] bool canPowerOff() const noexcept;
    [[nodiscard]] bool pending() const noexcept;
    [[nodiscard]] QString feedback() const;

    Q_INVOKABLE bool requestLock();
    Q_INVOKABLE bool requestLogout();
    Q_INVOKABLE bool requestSuspend();
    Q_INVOKABLE bool requestReboot();
    Q_INVOKABLE bool requestPowerOff();
    Q_INVOKABLE void clearFeedback();

Q_SIGNALS:
    void availabilityChanged();
    void pendingChanged();
    void feedbackChanged();
    void actionFinished(const QindaQt::Services::SessionActions::SessionActionResult &result);

private:
    struct RefreshQuery;
    struct PendingAction {
        quint64 requestId = 0;
        SessionAction action = SessionAction::Lock;
        QString owner;
        quint64 authorityEpoch = 0;
        bool mutationDispatched = false;
    };

    void scheduleRefresh();
    void refreshAvailability();
    void completeRefresh(const std::shared_ptr<RefreshQuery> &query);
    void publishAvailability(const SessionActionAvailability &availability);
    [[nodiscard]] bool requestAction(SessionAction action);
    void authorizeLock();
    void authorizeLogout();
    void authorizeLogind();
    void dispatchMutation();
    void completePending(ActionStatus status, const QString &reasonCode);
    void publishFeedback(QString feedback);
    [[nodiscard]] bool cachedAvailable(SessionAction action) const noexcept;
    [[nodiscard]] QString currentOwner(SessionAction action) const;
    [[nodiscard]] quint64 authorityEpoch(SessionAction action) const noexcept;
    [[nodiscard]] bool authorityMatches(const PendingAction &request) const;
    void advanceAuthorityEpoch(SessionAction action);

    QDBusConnection m_sessionBus;
    QDBusConnection m_systemBus;
    QDBusServiceWatcher *m_sessionWatcher = nullptr;
    QDBusServiceWatcher *m_screenSaverWatcher = nullptr;
    QDBusServiceWatcher *m_logindWatcher = nullptr;
    QTimer m_refreshDebounce;
    QTimer m_refreshDeadline;
    QTimer m_actionDeadline;
    SessionActionAvailability m_availability;
    std::optional<PendingAction> m_pending;
    QString m_feedback;
    quint64 m_refreshSerial = 0;
    quint64 m_nextRequestId = 0;
    quint64 m_screenSaverEpoch = 0;
    quint64 m_sessionEpoch = 0;
    quint64 m_logindEpoch = 0;
    bool m_running = false;
};

} // namespace QindaQt::Services::SessionActions

Q_DECLARE_METATYPE(QindaQt::Services::SessionActions::SessionAction)
Q_DECLARE_METATYPE(QindaQt::Services::SessionActions::ActionStatus)
Q_DECLARE_METATYPE(QindaQt::Services::SessionActions::SessionActionAvailability)
Q_DECLARE_METATYPE(QindaQt::Services::SessionActions::SessionActionResult)
