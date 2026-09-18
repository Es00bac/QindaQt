// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/session/desktop_controls/battery_notification_policy.h"

using QindaQt::Power::CompositeBattery;
using QindaQt::Power::Snapshot;
using QindaQt::Power::WarningLevel;

namespace QindaQt::Session::DesktopControls {
namespace {

QString durationText(const qint64 seconds) {
    if (seconds < 60) {
        return QObject::tr("under a minute");
    }
    const qint64 hours = seconds / 3600;
    const qint64 minutes = (seconds % 3600) / 60;
    if (hours == 0) {
        return QObject::tr("%1 minutes").arg(minutes);
    }
    if (minutes == 0) {
        return QObject::tr("%1 hours").arg(hours);
    }
    return QObject::tr("%1 hours %2 minutes").arg(hours).arg(minutes);
}

QString body(const CompositeBattery &composite) {
    QStringList parts;
    if (composite.percentageKnown) {
        parts.append(QObject::tr("%1%")
                          .arg(qRound(qBound(0.0, composite.percentage, 100.0))));
    }
    if (composite.timeToEmptyKnown && composite.timeToEmptySeconds >= 0) {
        parts.append(
            QObject::tr("%1 remaining").arg(durationText(composite.timeToEmptySeconds)));
    }
    return parts.join(QStringLiteral(" · "));
}

} // namespace

BatteryNotificationPolicy::BatteryNotificationPolicy(
    QindaQt::Power::PowerClient &client, FreedesktopFeedbackNotifier &notifier,
    QObject *parent)
    : QObject(parent), m_client(client), m_notifier(notifier)
{
}

BatteryNotificationPolicy::~BatteryNotificationPolicy() = default;

void BatteryNotificationPolicy::start()
{
    connect(&m_client, &QindaQt::Power::PowerClient::snapshotChanged, this,
            &BatteryNotificationPolicy::onSnapshotChanged);
}

void BatteryNotificationPolicy::onSnapshotChanged(const Snapshot &snapshot)
{
    const CompositeBattery &composite = snapshot.composite;

    // AGENT-GUARD: a desktop with no battery, or a battery UPower cannot
    // currently report on, must never notify; it also resets the edge so a
    // battery that reappears (hot-plugged UPS, resumed UPower) starts clean.
    if (!composite.present) {
        m_lastNotifiedLevel = WarningLevel::None;
        return;
    }

    const WarningLevel level = composite.warning;

    // AGENT-GUARD: fire once per crossing into Low/Critical/Action, not once
    // per snapshot tick (the client republishes on every poll). Any level at
    // or below the last notified one is not a new crossing: either the
    // charge state has not worsened, or it has already been announced. A
    // level below Low (charging back up, or plugged in) clears the edge so
    // the next discharge into Low notifies again.
    if (level == WarningLevel::Low || level == WarningLevel::Critical
        || level == WarningLevel::Action) {
        if (static_cast<int>(level) <= static_cast<int>(m_lastNotifiedLevel)) {
            return;
        }
        const bool critical = level != WarningLevel::Low;
        const QString summary = level == WarningLevel::Low
            ? tr("Battery low")
            : level == WarningLevel::Critical ? tr("Battery critical")
                                              : tr("Battery critically low");
        const QString icon = level == WarningLevel::Low
            ? QStringLiteral("battery-low")
            : QStringLiteral("battery-caution");
        m_notifier.showBattery(summary, body(composite), icon, critical);
        m_lastNotifiedLevel = level;
        return;
    }

    m_lastNotifiedLevel = WarningLevel::None;
}

} // namespace QindaQt::Session::DesktopControls
