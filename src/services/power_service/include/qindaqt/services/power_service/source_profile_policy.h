// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/power_service/power_service_coordinator.h>
#include <qindaqt/services/settings_client/settings_client.h>
#include <QtCore/QTimer>

namespace QindaQt::Power {

// Same-thread borrowed coordinator and public SettingsClient must outlive this
// object. Owns only a nonce-tagged hold; never changes ActiveProfile or releases
// another caller's hold. Unknown dispatched outcomes quarantine acquisition for
// this lifetime; confirmed disappearance still permits cleanup of our hold.
class SourceProfilePolicy final : public QObject {
    Q_OBJECT
public:
    SourceProfilePolicy(PowerServiceCoordinator &power,
                        Services::SettingsClient::SettingsClient &settings,
                        int operationTimeoutMilliseconds = 4000,
                        QObject *parent = nullptr);
    static QStringList settingsKeys();
    void setNativeAuthority(bool admitted);
    // Retry known refusals only. An uncertain dispatch cannot be made safe by
    // repeating the user's intent and remains quarantined until a new runtime.
    void retry();
    [[nodiscard]] bool quarantined() const noexcept { return m_quarantined; }
private:
    void schedule();
    void reconcile();
    QString desiredProfile() const;
    void dispatch(PowerServiceRequest request);
    void completed(quint64 id, const OperationResult &result);
    PowerServiceCoordinator &m_power;
    Services::SettingsClient::SettingsClient &m_settings;
    QTimer m_deadline;
    QString m_reason;
    QString m_requestedProfile;
    QString m_attemptKey;
    QString m_failedKey;
    Handle m_owned;
    quint64 m_operation = 0;
    quint64 m_epoch = 0;
    OperationKind m_kind = OperationKind::AcquireProfileHold;
    bool m_authority = false;
    bool m_scheduled = false;
    bool m_quarantined = false;
    bool m_awaitingObservation = false;
};
} // namespace QindaQt::Power
