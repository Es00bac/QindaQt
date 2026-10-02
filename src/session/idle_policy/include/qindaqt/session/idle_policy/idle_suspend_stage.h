// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/session/idle_policy/shared_preferences.h>
#include <QObject>
#include <functional>
namespace QindaQt::Platform::Idle { class IdleObservation; }
namespace QindaQt::Power { class PowerClient; }
namespace QindaQt::Session::IdlePolicy {
// Borrowed same-thread port. Implementations must use the existing Protected
// sleep boundary and return an owned request token; cancel never touches another
// caller's work. Capability completion is once-only, including authority loss.
class IdleSleepPort : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual bool available() const = 0;
    virtual void query(const QString &action, std::function<void(bool)> completion) = 0;
    virtual quint64 request(const QString &action) = 0;
    virtual void cancel(quint64 token) = 0;
Q_SIGNALS:
    void availabilityChanged();
    void requestFinished(quint64 token, bool uncertain);
};
class IdleSuspendStage final : public QObject {
    Q_OBJECT
public:
    using Preferences = std::function<std::optional<SharedIdlePreferences>()>;
    IdleSuspendStage(Platform::Idle::IdleObservation &idle, Power::PowerClient &power,
                     IdleSleepPort &sleep, Preferences preferences, QObject *parent = nullptr);
    ~IdleSuspendStage() override;
    void start();
    void stop();
    void refreshPreferences();
private:
    bool suppressed() const;
    void apply();
    void idleChanged();
    void cancel();
    Platform::Idle::IdleObservation &m_idle;
    Power::PowerClient &m_power;
    IdleSleepPort &m_sleep;
    Preferences m_preferences;
    std::optional<SharedIdlePreferences> m_current;
    QString m_owner;
    quint64 m_epoch = 0, m_serial = 0, m_token = 0;
    bool m_started = false, m_consumed = false, m_querying = false, m_submitting = false, m_quarantined = false;
    int m_timeout = 0;
};
}
