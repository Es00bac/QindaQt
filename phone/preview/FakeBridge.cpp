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

QStringList FakeBridge::inputScenarios()
{
    return {QStringLiteral("active"), QStringLiteral("pending"), QStringLiteral("denied"), QStringLiteral("locked"),
            QStringLiteral("running"), QStringLiteral("failed"), QStringLiteral("empty")};
}

void FakeBridge::setInputScenario(const QString &name)
{
    // Synthetic macro names only; the preview never reads the real library.
    auto row = [](const char *id, const char *label) {
        return QVariantMap{{QStringLiteral("id"), QString::fromLatin1(id)}, {QStringLiteral("name"), QString::fromLatin1(label)}};
    };
    m_macros = {row("copy", "Copy"), row("paste", "Paste"), row("snap-left", "Snap window left"),
                row("screenshot", "Screenshot area")};
    m_macroRunning.clear(); m_macroRunState.clear(); m_macroReason.clear(); m_macroLast.clear();
    m_inputState = name == QLatin1String("pending") ? QStringLiteral("pending")
        : name == QLatin1String("denied") ? QStringLiteral("denied")
        : name == QLatin1String("locked") ? QStringLiteral("locked") : QStringLiteral("active");
    if (name == QLatin1String("running")) { m_macroRunning = QStringLiteral("paste"); m_macroRunState = QStringLiteral("running"); }
    if (name == QLatin1String("failed")) { m_macroRunState = QStringLiteral("failed"); m_macroReason = QStringLiteral("input-unavailable"); }
    if (name == QLatin1String("empty")) m_macros.clear();
    emit inputChanged();
}
