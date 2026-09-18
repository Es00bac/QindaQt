// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "freedesktop_feedback_notifier.h"

#include <qindaqt/services/power_client/power_client.h>

#include <QObject>

namespace QindaQt::Session::DesktopControls {

// Edge-triggered low/critical battery notifications (Checkpoint L row 5).
// QindaQt names no percentage thresholds itself: `composite.warning` is
// UPower's own WarningLevel, already computed from
// PercentageLow/PercentageCritical/PercentageAction in UPower.conf (20/5/2
// on qinda-top). This policy only decides *when to speak*: once per crossing
// into Low or Critical, never once per snapshot tick, and it resets so a
// later re-discharge (after a partial charge, or after `Action` triggers a
// suspend) notifies again.
class BatteryNotificationPolicy final : public QObject {
    Q_OBJECT

public:
    BatteryNotificationPolicy(QindaQt::Power::PowerClient &client,
                              FreedesktopFeedbackNotifier &notifier,
                              QObject *parent = nullptr);
    ~BatteryNotificationPolicy() override;

    BatteryNotificationPolicy(const BatteryNotificationPolicy &) = delete;
    BatteryNotificationPolicy &operator=(const BatteryNotificationPolicy &) = delete;

    void start();

    // The most severe level notified since the last reset; exposed for
    // tests, not for presentation (nothing renders from this policy).
    [[nodiscard]] QindaQt::Power::WarningLevel lastNotifiedLevel() const noexcept {
        return m_lastNotifiedLevel;
    }

private Q_SLOTS:
    void onSnapshotChanged(const QindaQt::Power::Snapshot &snapshot);

private:
    QindaQt::Power::PowerClient &m_client;
    FreedesktopFeedbackNotifier &m_notifier;
    QindaQt::Power::WarningLevel m_lastNotifiedLevel =
        QindaQt::Power::WarningLevel::None;
};

} // namespace QindaQt::Session::DesktopControls
