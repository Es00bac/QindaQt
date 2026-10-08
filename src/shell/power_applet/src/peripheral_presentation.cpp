// SPDX-License-Identifier: GPL-3.0-or-later
#include "power_applet_controller.h"
#include <QtCore/QStringList>
#include <QtCore/QVariantMap>
#include <cmath>
namespace QindaQt::Shell::PowerApplet {
namespace {
QString kindName(Power::PeripheralKind kind)
{
    using K=Power::PeripheralKind;
    switch(kind) {
    case K::Mouse: return QObject::tr("Mouse");
    case K::Keyboard: return QObject::tr("Keyboard");
    case K::Controller: return QObject::tr("Controller");
    case K::Headset: return QObject::tr("Headset");
    case K::Speaker: return QObject::tr("Speaker");
    case K::Headphones: return QObject::tr("Headphones");
    case K::Phone: return QObject::tr("Phone");
    case K::Tablet: return QObject::tr("Tablet");
    case K::Computer: return QObject::tr("Computer");
    case K::Pen: return QObject::tr("Pen");
    case K::Touchpad: return QObject::tr("Touchpad");
    case K::MediaPlayer: return QObject::tr("Media player");
    case K::Remote: return QObject::tr("Remote");
    case K::Camera: return QObject::tr("Camera");
    case K::Wearable: return QObject::tr("Wearable");
    case K::Other: return QObject::tr("Device");
    }
    return QObject::tr("Device");
}
QString levelName(Power::BatteryLevel level)
{
    using L=Power::BatteryLevel;
    switch(level) {
    case L::Low: return QObject::tr("Low");
    case L::Critical: return QObject::tr("Critical");
    case L::Normal: return QObject::tr("Normal");
    case L::High: return QObject::tr("High");
    case L::Full: return QObject::tr("Full");
    default: return QObject::tr("Battery level unknown");
    }
}
QString stateName(Power::ChargeState state)
{
    using S=Power::ChargeState;
    switch(state) {
    case S::Charging: return QObject::tr("Charging");
    case S::Discharging: return QObject::tr("Discharging");
    case S::Empty: return QObject::tr("Empty");
    case S::FullyCharged: return QObject::tr("Fully charged");
    case S::PendingCharge: return QObject::tr("Waiting to charge");
    case S::PendingDischarge: return QObject::tr("Waiting to discharge");
    default: return {};
    }
}
}
void PowerAppletController::setPeripheralClient(Power::PeripheralClient *client)
{
    if (m_peripherals) disconnect(m_peripherals,nullptr,this,nullptr);
    m_peripherals=client;
    if (client) connect(client,&Power::PeripheralClient::changed,this,&PowerAppletController::stateChanged);
    Q_EMIT stateChanged();
}
QVariantList PowerAppletController::peripheralRows() const
{
    QVariantList rows;
    if (!m_powerReadGranted || !m_peripherals || !m_peripherals->hasSnapshot()) return rows;
    for (const auto &device:m_peripherals->snapshot().devices) {
        const QString kind=kindName(device.kind);
        QString name=device.model.isEmpty() ? device.vendor : device.model;
        if (name.isEmpty()) name=kind;
        QStringList details;
        details << (device.percentageKnown ? tr("%1%").arg(qRound(device.percentage)) : levelName(device.level));
        const QString state=stateName(device.state);
        if (!state.isEmpty()) details << state;
        if (!device.present) details << tr("Not present");
        if (device.timeToEmptyKnown) details << tr("%1 min remaining").arg((device.timeToEmptySeconds+59)/60);
        if (device.timeToFullKnown) details << tr("%1 min until full").arg((device.timeToFullSeconds+59)/60);
        const QString detail=details.join(QStringLiteral(" · "));
        rows.push_back(QVariantMap{{QStringLiteral("name"),name},{QStringLiteral("kind"),kind},
            {QStringLiteral("detail"),detail},{QStringLiteral("accessibleName"),name+QStringLiteral(", ")+kind+QStringLiteral(", ")+detail}});
    }
    return rows;
}
QString PowerAppletController::peripheralDiagnostic() const
{
    if (!m_powerReadGranted || !m_peripherals) return {};
    if (!m_peripherals->hasSnapshot()) return m_peripherals->reasonCode()==QStringLiteral("starting")
        ? tr("Device batteries are loading…") : tr("Device batteries are unavailable");
    const auto snapshot=m_peripherals->snapshot();
    if (snapshot.truncated) return tr("Showing up to 64 devices; %1 additional devices omitted").arg(snapshot.omittedCount);
    if (snapshot.omittedCount) return tr("%1 device batteries could not be read").arg(snapshot.omittedCount);
    if (snapshot.availability==Power::Availability::Unavailable) return tr("Device batteries are unavailable");
    if (snapshot.availability==Power::Availability::Starting) return tr("Device batteries are loading…");
    if (snapshot.availability==Power::Availability::Degraded) return tr("Device battery information is incomplete");
    return {};
}
}
