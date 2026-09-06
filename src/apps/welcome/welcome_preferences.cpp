// SPDX-License-Identifier: GPL-3.0-or-later
#include "welcome_preferences.h"

#include <QSettings>

namespace QindaQt::Apps::Welcome {
WelcomePreferences::WelcomePreferences(QObject *parent) : QObject(parent) {
    QSettings settings;
    m_showAtNextLaunch = settings.value(QStringLiteral("welcome/showAtNextLaunch"), true).toBool();
}
bool WelcomePreferences::showAtNextLaunch() const { return m_showAtNextLaunch; }
void WelcomePreferences::setShowAtNextLaunch(bool enabled) {
    if (m_showAtNextLaunch == enabled) return;
    m_showAtNextLaunch = enabled;
    QSettings().setValue(QStringLiteral("welcome/showAtNextLaunch"), enabled);
    Q_EMIT showAtNextLaunchChanged();
}
}
