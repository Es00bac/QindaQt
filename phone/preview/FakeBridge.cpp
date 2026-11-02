// SPDX-License-Identifier: LGPL-3.0-or-later
#include "FakeBridge.h"

QStringList FakeBridge::scenarios()
{
    return {QStringLiteral("offline"), QStringLiteral("found"), QStringLiteral("confirm"),
            QStringLiteral("stale"), QStringLiteral("ready"), QStringLiteral("perms-partial")};
}

void FakeBridge::setScenario(const QString &name)
{
    // Synthetic values only: the preview never reads the phone or the desktop.
    m_phase = QStringLiteral("disconnected");
    m_desktopName.clear(); m_code.clear(); m_pairedName.clear(); m_transport.clear();
    m_message = QStringLiteral("No desktop reachable");
    m_paired = false; m_battery = 76; m_charging = false;
    m_perm = {{QStringLiteral("notificationAccess"), QStringLiteral("later")},
              {QStringLiteral("sms"), QStringLiteral("later")},
              {QStringLiteral("contacts"), QStringLiteral("later")},
              {QStringLiteral("postNotifications"), QStringLiteral("denied")},
              {QStringLiteral("batteryOptimisation"), QStringLiteral("denied")}};
    if (name == QLatin1String("found")) {
        m_phase = QStringLiteral("connected"); m_desktopName = QStringLiteral("qinda");
        m_transport = QStringLiteral("usb"); m_message = QStringLiteral("Desktop reachable");
    } else if (name == QLatin1String("confirm")) {
        m_phase = QStringLiteral("confirm"); m_desktopName = QStringLiteral("qinda");
        m_transport = QStringLiteral("usb"); m_code = QStringLiteral("483920");
        m_message = QStringLiteral("Compare the code");
    } else if (name == QLatin1String("stale")) {
        m_phase = QStringLiteral("stale"); m_desktopName = QStringLiteral("qinda");
        m_transport = QStringLiteral("usb");
        m_message = QStringLiteral("The desktop presented a different identity than the one saved");
    } else if (name == QLatin1String("ready") || name == QLatin1String("perms-partial")) {
        m_phase = QStringLiteral("ready"); m_desktopName = QStringLiteral("qinda");
        m_pairedName = QStringLiteral("qinda"); m_paired = true;
        m_transport = QStringLiteral("usb"); m_message = QStringLiteral("Sending battery and network status");
        m_charging = true; m_battery = 82;
        if (name == QLatin1String("perms-partial")) {
            m_perm[QStringLiteral("postNotifications")] = QStringLiteral("granted");
            m_perm[QStringLiteral("notificationAccess")] = QStringLiteral("denied");
            m_perm[QStringLiteral("sms")] = QStringLiteral("denied");
            m_perm[QStringLiteral("contacts")] = QStringLiteral("granted");
        }
    }
    emit changed();
}

void FakeBridge::pair() { setScenario(QStringLiteral("confirm")); }
void FakeBridge::pairAgain() { setScenario(QStringLiteral("confirm")); }
void FakeBridge::accept() { setScenario(QStringLiteral("ready")); }
void FakeBridge::reject() { setScenario(QStringLiteral("found")); }
void FakeBridge::forget() { setScenario(QStringLiteral("found")); }
