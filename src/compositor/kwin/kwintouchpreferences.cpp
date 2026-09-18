// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwintouchpreferences.h"

#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/settings_client/settings_client.h"

namespace QindaQt::Compositor::KWinIntegration {

KWinTouchPreferences::KWinTouchPreferences(QDBusConnection bus, QObject *parent)
    : QObject(parent)
    , m_transport(std::make_unique<Services::SettingsClient::QtSettingsTransport>(bus))
    , m_settings(std::make_unique<Services::SettingsClient::SettingsClient>(*m_transport,
                                                                            TouchPreferences::settingsKeys()))
{
    connect(m_settings.get(), &Services::SettingsClient::SettingsClient::snapshotChanged, this,
            &KWinTouchPreferences::refresh);
    QString error;
    if (!m_settings->start(&error)) {
        qWarning("QindaQt compositor: touch preferences unavailable (%s); defaults apply", qPrintable(error));
    }
}

KWinTouchPreferences::~KWinTouchPreferences() = default;

void KWinTouchPreferences::refresh()
{
    const auto &snapshot = m_settings->snapshot();
    const TouchPreferences next = snapshot ? TouchPreferences::fromSettingsValues(snapshot->values) : TouchPreferences{};
    if (next == m_preferences) {
        return;
    }
    m_preferences = next;
    Q_EMIT preferencesChanged();
}

} // namespace QindaQt::Compositor::KWinIntegration
